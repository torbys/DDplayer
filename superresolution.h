#ifndef SUPERRESOLUTION_H
#define SUPERRESOLUTION_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "ffplay_def.h"

// 前向声明 ONNX 类型
namespace Ort {
class Env;
class SessionOptions;
class Session;
class MemoryInfo;
class Value;
class AllocatorWithDefaultOptions;
class RunOptions;
}

// 归一化类型（每个模型可能不同）
enum class NormType {
    RANGE_0_1,      // [0, 1]  — HFA2kCompact 等
    RANGE_NEG1_1    // [-1, 1] — AnimeJaNai V3 等
};

// 模型信息结构体
struct ModelInfo {
    std::string name;           // 显示名称
    std::string filePath;       // ONNX 文件路径
    int scale;                  // 放大倍数
    NormType normType;          // 归一化类型
};

// 预定义可用模型列表
static const ModelInfo AVAILABLE_MODELS[] = {
    {"HFA2kCompact x2", "models/Phhofm_HFA2kCompact_x2_fp32_opset14.onnx", 2, NormType::RANGE_0_1},
    {"AnimeJaNai V3-L1", "models/animejanaiV3-HD-L1.onnx", 2, NormType::RANGE_NEG1_1},
    {"AnimeJaNai V3-L2", "models/animejanaiV3-HD-L2.onnx", 2, NormType::RANGE_NEG1_1},
    {"AnimeJaNai V3-L3", "models/animejanaiV3-HD-L3.onnx", 2, NormType::RANGE_NEG1_1},
};
static const int MODEL_COUNT = sizeof(AVAILABLE_MODELS) / sizeof(AVAILABLE_MODELS[0]);

// 获取给定 NormType 对应的归一化参数
inline void getNormParams(NormType type, float &scale, float &bias) {
    if (type == NormType::RANGE_NEG1_1) {
        scale = 2.0f; bias = -1.0f;     // [0,1] → [-1,1]
    } else {
        scale = 1.0f; bias = 0.0f;      // [0,1] → [0,1]
    }
}
inline void getDenormParams(NormType type, float &scale, float &bias) {
    if (type == NormType::RANGE_NEG1_1) {
        scale = 0.5f; bias = 0.5f;      // [-1,1] → [0,1]
    } else {
        scale = 1.0f; bias = 0.0f;      // [0,1] → [0,1]
    }
}

class SuperResolution
{
public:
    explicit SuperResolution(const std::string &modelDir = "models");
    ~SuperResolution();

    // 初始化 ONNX Runtime 环境
    bool initialize();

    // 加载指定模型
    bool loadModel(int modelIndex);
    bool loadModelByPath(const std::string &path);

    // ===== 核心超分入口（带显示感知） =====
    // 输入 AVFrame(YUV420P)，输出超分后的 YUV420P 数据
    // 返回: true  = 超分已应用，输出在 m_outYuv/m_outW/m_outH
    //       false = 跳过（源 ≥ 显示区域 或 上一帧还在推理）
    bool processFrame(AVFrame *inputFrame);

    // 获取超分后的 YUV420P 数据（processFrame 返回 true 后调用）
    const std::vector<uint8_t>& outputYuvBuffer() const { return m_outYuv; }
    int outputWidth() const { return m_outW; }
    int outputHeight() const { return m_outH; }
    bool outputReady() const { return !m_outYuv.empty(); }

    // 状态查询
    bool isReady() const;
    int currentModelIndex() const { return m_currentModelIndex; }
    int currentScale() const { return m_scale; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled && isReady(); }
    float lastInferenceTimeMs() const { return m_lastInferenceMs; }

    // 设置显示器分辨率（用于智能判断是否需要超分）
    void setDisplaySize(int width, int height) { m_displayW = width; m_displayH = height; }

    // 设置视频渲染区域尺寸（用于更精确的预降分辨率决策）
    void setRenderAreaSize(int width, int height) { m_renderW = width; m_renderH = height; }

    // 设置是否启用 fp16 半精度推理
    void setEnableFP16(bool enabled) { m_enableFP16 = enabled; }

private:
    // ========== 预处理/后处理（使用 FFmpeg sws_scale 加速）==========
    // YUV420P → RGB24 + Float32 CHW + 归一化（写入预分配 m_inputBuf）
    void preprocessWithSws(AVFrame *frame, int &outW, int &outH, NormType norm);

    // Float32 CHW 反归一化 → YUV420P
    void postprocessToYuv(const float *outputData, int width, int height,
                          NormType norm);

    // ========== ONNX 推理核心（结果写入预分配 m_outputBuf） ==========
    bool runInference(const float *inputData, int inputHeight, int inputWidth,
                      int &outputHeight, int &outputWidth);

    // ========== 辅助 ==========
    void releaseSession();
    // 计算目标 SR 输入尺寸（考虑显示区域分辨率和放大倍数）
    void calcTargetSize(int srcW, int srcH, int dspW, int dspH,
                        int &srW, int &srH, bool &skip);

    // ONNX 对象
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::SessionOptions> m_sessionOpts;
    std::unique_ptr<Ort::Session> m_session;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;

    // 模型状态
    bool m_initialized = false;
    bool m_enabled = true;
    bool m_enableFP16 = true;       // 默认开启 fp16
    int m_currentModelIndex = -1;
    int m_scale = 2;
    NormType m_currentNorm = NormType::RANGE_NEG1_1;

    // 显示器分辨率（外部设置）
    int m_displayW = 0;
    int m_displayH = 0;

    // 视频渲染区域尺寸（用于更精确的预降分辨率决策）
    int m_renderW = 0;
    int m_renderH = 0;

    // 推理尺寸缓存
    std::vector<int64_t> m_inputShape;
    std::vector<int64_t> m_outputShape;
    std::string m_inputName;
    std::string m_outputName;

    // 性能统计
    float m_lastInferenceMs = 0.0f;

    // 模型文件目录
    std::string m_modelDir;

    // 输出缓冲（processFrame 的结果，YUV420P planar）
    std::vector<uint8_t> m_outYuv;
    int m_outW = 0, m_outH = 0;

    // sws_scale 上下文缓存（复用避免重复创建）
    struct SwsContext *m_swsCtx = nullptr;
    int m_swsSrcW = 0, m_swsSrcH = 0;

    // ===== 预分配缓冲区（避免每帧 new/delete） =====
    std::vector<float> m_inputBuf;       // 预处理输出：float32 CHW
    std::vector<uint8_t> m_rgbBuf;       // sws_scale 中间缓冲：RGB24
    std::vector<float> m_outputBuf;      // ONNX 推理输出：float32 CHW
    int m_bufInputW = 0, m_bufInputH = 0;   // 当前缓冲区对应的输入尺寸

    // 线程安全
    mutable std::mutex m_mutex;
    std::atomic<bool> m_busy{false};
};

#endif // SUPERRESOLUTION_H
