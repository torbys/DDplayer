#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QMutex>
#include <QQueue>
#include <QMatrix4x4>
#include "ffplay_def.h"
#include "superresolution.h"

// 视频帧包装，用于线程安全传递
struct VideoFrame {
    AVFrame *frame = nullptr;
    double pts = 0;
    double duration = 0;
    int width = 0;
    int height = 0;

    VideoFrame() {}
    VideoFrame(AVFrame *f, double p, double d) : frame(f), pts(p), duration(d) {
        if (frame) {
            width = frame->width;
            height = frame->height;
        }
    }

    void release() {
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
    }
};

class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit VideoGLWidget(QWidget *parent = nullptr);
    ~VideoGLWidget();

    // 供外部线程调用，将帧加入渲染队列（线程安全）
    int presentFrame(const Frame *vp);

    // 设置视频显示区域保持比例
    void setKeepAspectRatio(bool keep);

    // 清除待渲染帧队列
    void clearFrames();

    // 设置滤镜参数
    void setFilterParams(float brightness, float contrast, float saturation, float blur);
    void setFilterType(int type);

    // ===== AI 超分相关 =====
    void setSREnabled(bool enabled);
    bool loadSRModel(int modelIndex);

    bool isSREnabled() const { return m_srEnabled; }
    int currentSRModel() const;
    float lastSRInferenceMs() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void initShaders();

    // 更新纹理数据（支持 YUV420P 直接上传）
    void updateTextures(AVFrame *frame);

    // 计算适应窗口的视频显示区域
    QRect calculateTargetRect();

    // 检测显示器分辨率并更新超分引擎
    void updateDisplaySize();

    // OpenGL资源
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_ebo;
    QOpenGLVertexArrayObject m_vao;

    // YUV纹理
    GLuint m_textureY = 0;
    GLuint m_textureU = 0;
    GLuint m_textureV = 0;

    // 当前帧尺寸
    int m_frameWidth = 0;
    int m_frameHeight = 0;

    // 视频帧队列
    QMutex m_frameMutex;
    QQueue<VideoFrame> m_frameQueue;
    static const int MAX_FRAME_QUEUE_SIZE = 5;

    // 当前显示的帧
    VideoFrame m_currentFrame;

    QMatrix4x4 m_transformMatrix;

    void calculateAspectRatioTransform(int w, int h);

    // 显示设置
    bool m_keepAspectRatio = true;
    bool m_needUpdateTexture = false;

    // 滤镜参数
    float m_brightness = 0.0f;
    float m_contrast = 0.0f;
    float m_saturation = 0.0f;
    float m_blur = 0.0f;
    int m_filterType = 0;

    // AI 超分模块
    SuperResolution m_sr;
    bool m_srEnabled = false;
    bool m_srInitialized = false;
};

#endif // VIDEOGLWIDGET_H
