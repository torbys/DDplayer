#include "superresolution.h"
#include <onnxruntime_cxx_api.h>
#include <chrono>
#include <QDebug>
#include <QString>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>
#include <cstring>
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"

// ============================================================
// 构造/析构
// ============================================================

SuperResolution::SuperResolution(const std::string &modelDir)
    : m_modelDir(modelDir)
{
}

SuperResolution::~SuperResolution()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    releaseSession();
    m_sessionOpts.reset();
    m_env.reset();
}

// ============================================================
// 初始化 ONNX Runtime 环境
// ============================================================

bool SuperResolution::initialize()
{
    if (m_initialized) return true;

    try {
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "DDPlayer_SR");

        m_sessionOpts = std::make_unique<Ort::SessionOptions>();
        m_sessionOpts->SetIntraOpNumThreads(4);
        m_sessionOpts->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        m_sessionOpts->SetExecutionMode(ORT_SEQUENTIAL);

        // ===== fp16 半精度推理配置 =====
        if (m_enableFP16) {
            m_sessionOpts->AddConfigEntry("session.enable_fp16", "1");
            m_sessionOpts->AddConfigEntry("session.set_denormal_as_zero", "1");
            qDebug() << "SR: fp16 半精度推理已启用 (session.enable_fp16=1)";
        }

        // ===== CUDA Provider（ONNX Runtime GPU 加速，不需要 nvcc） =====
        try {
            OrtCUDAProviderOptions cuda_options;
            cuda_options.device_id = 0;
            cuda_options.gpu_mem_limit = 6442450944;  // 6GB
            cuda_options.arena_extend_strategy = 0;  // kNextPowerOfTwo
            cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
            cuda_options.do_copy_in_default_stream = 1;

            m_sessionOpts->AppendExecutionProvider_CUDA(cuda_options);
            qDebug() << "SR: CUDA GPU Provider 已启用";
        } catch (const Ort::Exception &e) {
            qDebug() << "SR: CUDA provider 设置失败，将使用 CPU:" << e.what();
        } catch (...) {
            qDebug() << "SR: CUDA provider 设置失败，将使用 CPU";
        }

        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        // 自动探测显示器分辨率
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QSize screenSize = screen->size();
            m_displayW = screenSize.width();
            m_displayH = screenSize.height();
            qDebug() << "SR: 检测到显示器分辨率:" << m_displayW << "x" << m_displayH;
        }

        m_initialized = true;
        qDebug() << "SR: ONNX Runtime 初始化成功";
        return true;

    } catch (const Ort::Exception &e) {
        qCritical() << "SR: ONNX 初始化异常:" << e.what();
        return false;
    } catch (const std::exception &e) {
        qCritical() << "SR: 初始化异常:" << e.what();
        return false;
    }
}

// ============================================================
// 模型加载
// ============================================================

bool SuperResolution::loadModel(int modelIndex)
{
    if (!m_initialized && !initialize()) return false;
    if (modelIndex < 0 || modelIndex >= MODEL_COUNT) {
        qCritical() << "SR: 无效的模型索引" << modelIndex;
        return false;
    }
    const ModelInfo &model = AVAILABLE_MODELS[modelIndex];
    m_currentNorm = model.normType;
    return loadModelByPath(model.filePath);
}

bool SuperResolution::loadModelByPath(const std::string &path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    releaseSession();

    try {
        QString qPath = QString::fromStdString(path);
        if (QDir::isRelativePath(qPath)) {
            QStringList candidateDirs;
            candidateDirs << QCoreApplication::applicationDirPath();
            candidateDirs << QDir(QCoreApplication::applicationDirPath()).filePath("../..");
            candidateDirs << QDir(QCoreApplication::applicationDirPath()).filePath("..");
            candidateDirs << QDir::currentPath();

            QString foundPath;
            for (const QString &dir : candidateDirs) {
                QString testPath = QDir(dir).absoluteFilePath(qPath);
                if (QFile::exists(testPath)) {
                    foundPath = testPath;
                    break;
                }
            }
            if (!foundPath.isEmpty()) {
                qPath = foundPath;
            } else {
                qPath = QDir(candidateDirs.first()).absoluteFilePath(qPath);
            }
        }
        std::wstring widePath = qPath.toStdWString();

        auto start = std::chrono::high_resolution_clock::now();
        m_session = std::make_unique<Ort::Session>(*m_env, widePath.c_str(), *m_sessionOpts);
        auto end = std::chrono::high_resolution_clock::now();
        float ms = std::chrono::duration<float, std::milli>(end - start).count();
        qDebug() << "SR: 模型加载耗时" << ms << "ms -" << qPath;

        // 获取输入/输出信息
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = m_session->GetInputNameAllocated(0, allocator);
        m_inputName = inputName.get();
        auto outputName = m_session->GetOutputNameAllocated(0, allocator);
        m_outputName = outputName.get();

        auto inputTypeInfo = m_session->GetInputTypeInfo(0);
        auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
        m_inputShape = inputTensorInfo.GetShape();

        auto outputTypeInfo = m_session->GetOutputTypeInfo(0);
        auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
        m_outputShape = outputTensorInfo.GetShape();

        qDebug() << "SR: 输入名称=" << m_inputName.c_str()
                 << " 形状=[" << m_inputShape[0] << "," << m_inputShape[1] << ","
                 << m_inputShape[2] << "," << m_inputShape[3] << "]";
        qDebug() << "SR: 输出名称=" << m_outputName.c_str()
                 << " 形状=[" << m_outputShape[0] << "," << m_outputShape[1] << ","
                 << m_outputShape[2] << "," << m_outputShape[3] << "]";

        if (m_outputShape.size() >= 4 && m_inputShape.size() >= 4 &&
            m_inputShape[2] > 0 && m_inputShape[3] > 0) {
            int outH = static_cast<int>(m_outputShape[2]);
            int inH = static_cast<int>(m_inputShape[2]);
            m_scale = outH / inH;
        }

        qDebug() << "SR: 模型加载成功，放大倍数 x" << m_scale
                 << "归一化:" << (m_currentNorm == NormType::RANGE_NEG1_1 ? "[-1,1]" : "[0,1]");
        return true;

    } catch (const Ort::Exception &e) {
        qCritical() << "SR: 模型加载失败:" << e.what();
        return false;
    }
}

void SuperResolution::releaseSession()
{
    m_session.reset();
    m_currentModelIndex = -1;
}

// ============================================================
// 显示感知：计算目标 SR 输入尺寸
// ============================================================

void SuperResolution::calcTargetSize(int srcW, int srcH, int dspW, int dspH,
                                     int &srW, int &srH, bool &skip)
{
    skip = false;

    // 实际显示区域尺寸
    int actualW = (m_renderW > 0) ? m_renderW : dspW;
    int actualH = (m_renderH > 0) ? m_renderH : dspH;

    // 没有显示区域信息，默认做超分
    if (actualW <= 0 || actualH <= 0) {
        srW = srcW;
        srH = srcH;
        return;
    }

    // ===== 规则1：源视频 ≥ 显示区域 → 已经够清晰，跳过超分 =====
    if (srcH >= actualH && srcW >= actualW) {
        qDebug() << "SR: 跳过超分（源" << srcW << "x" << srcH
                 << "≥ 显示" << actualW << "x" << actualH << "）";
        skip = true;
        return;
    }

    // ===== 规则2：源视频 < 显示区域 → 直接用原始分辨率做超分，不压缩 =====
    srW = srcW;
    srH = srcH;

    qDebug() << "SR: 直接超分" << srcW << "x" << srcH
             << "(< 显示" << actualW << "x" << actualH
             << ", 输出" << (srcW * m_scale) << "x" << (srcH * m_scale) << ")";
}

// ============================================================
// 核心入口：processFrame
// ============================================================

bool SuperResolution::processFrame(AVFrame *inputFrame)
{
    if (!isEnabled() || !inputFrame || !m_session) return false;

    // ===== 帧复用策略：上一帧还在推理中，返回上一次成功的结果 =====
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true)) {
        // 推理忙：如果已有成功结果，复用上一帧（比丢帧/黑帧好）
        return outputReady();
    }
    struct BusyGuard { std::atomic<bool> &flag; ~BusyGuard() { flag.store(false); } } guard{m_busy};

    int srcW = inputFrame->width;
    int srcH = inputFrame->height;

    // ===== 1. 显示感知决策 =====
    int srInputW, srInputH;
    bool skip = false;
    calcTargetSize(srcW, srcH, m_displayW, m_displayH, srInputW, srInputH, skip);
    if (skip) {
        m_outYuv.clear();
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        auto t0 = std::chrono::high_resolution_clock::now();

        AVFrame *procFrame = inputFrame;
        AVFrame *downscaledFrame = nullptr;

        // ===== 2. 预降分辨率（如果需要） =====
        bool needDownscale = (srInputW != srcW || srInputH != srcH) && (srInputH < srcH);
        if (needDownscale) {
            downscaledFrame = av_frame_alloc();
            downscaledFrame->format = AV_PIX_FMT_YUV420P;
            downscaledFrame->width = srInputW;
            downscaledFrame->height = srInputH;
            av_frame_get_buffer(downscaledFrame, 0);

            SwsContext *downCtx = sws_getContext(
                srcW, srcH, AV_PIX_FMT_YUV420P,
                srInputW, srInputH, AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (downCtx) {
                sws_scale(downCtx,
                          inputFrame->data, inputFrame->linesize, 0, srcH,
                          downscaledFrame->data, downscaledFrame->linesize);
                sws_freeContext(downCtx);
            }
            procFrame = downscaledFrame;
        }

        // ===== 3. 预处理：sws_scale(YUV→RGB) + 归一化 + CHW（写入预分配缓冲区）=====
        int inferW = procFrame->width;
        int inferH = procFrame->height;
        preprocessWithSws(procFrame, inferW, inferH, m_currentNorm);

        if (downscaledFrame) av_frame_free(&downscaledFrame);

        if (m_inputBuf.empty()) return false;

        auto tPre = std::chrono::high_resolution_clock::now();

        // ===== 4. ONNX 推理（写入预分配缓冲区） =====
        int onnxOutH, onnxOutW;
        bool inferOk = runInference(m_inputBuf.data(), inferH, inferW, onnxOutH, onnxOutW);

        auto tInf = std::chrono::high_resolution_clock::now();
        m_lastInferenceMs = std::chrono::duration<float, std::milli>(tInf - tPre).count();

        if (!inferOk || m_outputBuf.empty()) {
            qWarning() << "SR: 推理失败";
            return false;
        }

        int outW = onnxOutW;
        int outH = onnxOutH;

        // ===== 5. 后处理：反归一化 + CHW → YUV420P =====
        postprocessToYuv(m_outputBuf.data(), outW, outH, m_currentNorm);

        if (m_outYuv.empty()) return false;

        m_outW = outW;
        m_outH = outH;

        auto tEnd = std::chrono::high_resolution_clock::now();
        float totalMs = std::chrono::duration<float, std::milli>(tEnd - t0).count();
        float preMs = std::chrono::duration<float, std::milli>(tPre - t0).count();

        qDebug() << "SR:" << srInputW << "x" << srInputH
                 << "→" << outW << "x" << outH
                 << "| 预=" << QString::number(preMs, 'f', 1).toStdString()
                 << "ms 推=" << QString::number(m_lastInferenceMs, 'f', 1).toStdString()
                 << "ms 总=" << QString::number(totalMs, 'f', 1).toStdString() << "ms";

        return true;

    } catch (const std::exception &e) {
        qCritical() << "SR: processFrame 异常:" << e.what();
        return false;
    }
}

bool SuperResolution::isReady() const
{
    return m_initialized && m_session != nullptr;
}

// ============================================================
// 预处理：sws_scale(YUV→RGB24) + 归一化 + CHW 重排
// 结果写入预分配的 m_inputBuf（避免每帧分配内存）
// ============================================================

void SuperResolution::preprocessWithSws(AVFrame *frame, int &outW, int &outH, NormType norm)
{
    outW = frame->width;
    outH = frame->height;
    int totalPixels = outW * outH;
    int neededSize = totalPixels * 3;

    // 预分配/复用输入缓冲区
    if ((int)m_inputBuf.size() < neededSize || m_bufInputW != outW || m_bufInputH != outH) {
        m_inputBuf.resize(neededSize);
        m_bufInputW = outW;
        m_bufInputH = outH;
    }

    float normScale, normBias;
    getNormParams(norm, normScale, normBias);

    // Step 1: sws_scale YUV420P → RGB24（FFmpeg SIMD 加速）
    if (!m_swsCtx || m_swsSrcW != outW || m_swsSrcH != outH) {
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        m_swsCtx = sws_getContext(outW, outH, AV_PIX_FMT_YUV420P,
                                   outW, outH, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsSrcW = outW;
        m_swsSrcH = outH;
    }

    // 复用 RGB 缓冲区
    int rgbStride = outW * 3;
    rgbStride = (rgbStride + 31) & ~31;  // 32 字节对齐
    int rgbSize = rgbStride * outH;
    if ((int)m_rgbBuf.size() < rgbSize) {
        m_rgbBuf.resize(rgbSize);
    }

    uint8_t *dstSlice[1] = { m_rgbBuf.data() };
    int dstLinesize[1] = { rgbStride };

    sws_scale(m_swsCtx,
              frame->data, frame->linesize, 0, outH,
              dstSlice, dstLinesize);

    // Step 2: RGB24 → float32 CHW + 归一化（单次遍历，写入预分配缓冲区）
    float *data = m_inputBuf.data();
    for (int j = 0; j < outH; j++) {
        const uint8_t *row = m_rgbBuf.data() + j * rgbStride;
        for (int i = 0; i < outW; i++) {
            int idx = j * outW + i;
            int pixIdx = i * 3;
            data[idx]               = (row[pixIdx]     / 255.0f) * normScale + normBias;
            data[totalPixels + idx] = (row[pixIdx + 1] / 255.0f) * normScale + normBias;
            data[totalPixels * 2 + idx] = (row[pixIdx + 2] / 255.0f) * normScale + normBias;
        }
    }
}

// ============================================================
// 后处理：Float32 CHW 反归一化 → YUV420P planar
// ============================================================

void SuperResolution::postprocessToYuv(const float *outputData, int width, int height,
                                       NormType norm)
{
    float denormScale, denormBias;
    getDenormParams(norm, denormScale, denormBias);

    int totalPixels = width * height;
    m_outYuv.resize(totalPixels * 3 / 2);

    uint8_t *yPlane = m_outYuv.data();
    uint8_t *uPlane = m_outYuv.data() + totalPixels;
    uint8_t *vPlane = m_outYuv.data() + totalPixels + totalPixels / 4;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int idx = j * width + i;

            // 反归一化 + clamp [0, 1]
            float r = outputData[idx]               * denormScale + denormBias;
            float g = outputData[totalPixels + idx] * denormScale + denormBias;
            float b = outputData[totalPixels * 2 + idx] * denormScale + denormBias;

            r = fmaxf(0.0f, fminf(1.0f, r));
            g = fmaxf(0.0f, fminf(1.0f, g));
            b = fmaxf(0.0f, fminf(1.0f, b));

            // RGB → YUV (BT.601)
            float yy = 0.299f * r + 0.587f * g + 0.114f * b;
            float uu = -0.168736f * r - 0.331264f * g + 0.5f * b + 0.5f;
            float vv = 0.5f * r - 0.418688f * g - 0.081312f * b + 0.5f;

            yy = fmaxf(0.0f, fminf(1.0f, yy));
            uu = fmaxf(0.0f, fminf(1.0f, uu));
            vv = fmaxf(0.0f, fminf(1.0f, vv));

            yPlane[j * width + i] = static_cast<uint8_t>(yy * 255.0f + 0.5f);
            // UV 只写偶数位置（4:2:0 subsampling）
            if ((j & 1) == 0 && (i & 1) == 0) {
                int uvIdx = (j >> 1) * (width >> 1) + (i >> 1);
                uPlane[uvIdx] = static_cast<uint8_t>(uu * 255.0f + 0.5f);
                vPlane[uvIdx] = static_cast<uint8_t>(vv * 255.0f + 0.5f);
            }
        }
    }
}

// ============================================================
// ONNX 推理核心
// ============================================================

bool SuperResolution::runInference(const float *inputData, int inputHeight, int inputWidth,
                                    int &outputHeight, int &outputWidth)
{
    std::vector<int64_t> inputDims = {1, 3, inputHeight, inputWidth};

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        *m_memoryInfo,
        const_cast<float *>(inputData),
        inputHeight * inputWidth * 3,
        inputDims.data(),
        inputDims.size());

    const char *inputNames[] = {m_inputName.c_str()};
    const char *outputNames[] = {m_outputName.c_str()};

    auto outputTensors = m_session->Run(
        Ort::RunOptions{nullptr},
        inputNames, &inputTensor, 1,
        outputNames, 1);

    auto &outputTensor = outputTensors[0];
    auto outputInfo = outputTensor.GetTensorTypeAndShapeInfo();
    auto outputDims = outputInfo.GetShape();

    outputHeight = static_cast<int>(outputDims[2]);
    outputWidth = static_cast<int>(outputDims[3]);

    const float *outputPtr = outputTensor.GetTensorData<float>();
    size_t outputSize = (size_t)outputHeight * outputWidth * 3;

    // 写入预分配缓冲区（避免每帧重新分配）
    if (m_outputBuf.size() < outputSize) {
        m_outputBuf.resize(outputSize);
    }
    memcpy(m_outputBuf.data(), outputPtr, outputSize * sizeof(float));

    return true;
}
