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

    // 设置滤镜参数（亮度、对比度、饱和度、模糊值，范围 -1.0 ~ 1.0）
    void setFilterParams(float brightness, float contrast, float saturation, float blur);

    // 设置滤镜类型（0=无，1=复古胶片，2=模糊效果，3=卡通，4=黑白，5=褐色怀旧，6=边缘检测，7=锐化，8=油画，9=水彩画）
    void setFilterType(int type);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    // 初始化着色器
    void initShaders();

    // 更新纹理数据
    void updateTextures(AVFrame *frame);

    // 计算适应窗口的视频显示区域
    QRect calculateTargetRect();

    // OpenGL资源
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_ebo;
    QOpenGLVertexArrayObject m_vao;

    // YUV纹理（使用3个单通道纹理分别存储Y、U、V）
    GLuint m_textureY = 0;
    GLuint m_textureU = 0;
    GLuint m_textureV = 0;

    // 当前帧尺寸
    int m_frameWidth = 0;
    int m_frameHeight = 0;

    // 视频帧队列（生产者-消费者模式，符合原代码逻辑）
    QMutex m_frameMutex;
    QQueue<VideoFrame> m_frameQueue;
    static const int MAX_FRAME_QUEUE_SIZE = 5;  // 控制延迟，参考原代码的VIDEO_PICTURE_QUEUE_SIZE

    // 当前显示的帧
    VideoFrame m_currentFrame;

    QMatrix4x4 m_transformMatrix;  // 添加：变换矩阵

    void calculateAspectRatioTransform(int w, int h);

    // 显示设置
    bool m_keepAspectRatio = true;

    // 标记是否需要更新纹理
    bool m_needUpdateTexture = false;

    // 滤镜参数
    float m_brightness = 0.0f;
    float m_contrast = 0.0f;
    float m_saturation = 0.0f;
    float m_blur = 0.0f;
    int m_filterType = 0;
};

#endif // VIDEOGLWIDGET_H
