#include "VideoGLWidget.h"
#include <QOpenGLShader>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>

// ============================================================
// 着色器（与之前保持一致）
// ============================================================
static const char *vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 vTexCoord;

    uniform mat4 transform;

    void main()
    {
        gl_Position = transform * vec4(aPos, 0.0, 1.0);
        vTexCoord = aTexCoord;
    }
)";

static const char *fragmentShaderSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 FragColor;

    uniform sampler2D textureY;
    uniform sampler2D textureU;
    uniform sampler2D textureV;
    uniform vec2 texSize;

    uniform float u_brightness;
    uniform float u_contrast;
    uniform float u_saturation;
    uniform float u_blur;

    uniform int u_filterType;

    vec3 yuvToRgb(float y, float u, float v) {
        float r = y + 1.402 * v;
        float g = y - 0.344136 * u - 0.714136 * v;
        float b = y + 1.772 * u;
        return clamp(vec3(r, g, b), 0.0, 1.0);
    }

    vec3 getPixelColor() {
        float y = texture(textureY, vTexCoord).r;
        float u = texture(textureU, vTexCoord).r - 0.5;
        float v = texture(textureV, vTexCoord).r - 0.5;
        return yuvToRgb(y, u, v);
    }

    vec3 gaussianBlur() {
        vec2 offset[9];
        offset[0] = vec2(-1.0, -1.0); offset[1] = vec2(0.0, -1.0); offset[2] = vec2(1.0, -1.0);
        offset[3] = vec2(-1.0,  0.0); offset[4] = vec2(0.0,  0.0); offset[5] = vec2(1.0,  0.0);
        offset[6] = vec2(-1.0,  1.0); offset[7] = vec2(0.0,  1.0); offset[8] = vec2(1.0,  1.0);

        float kernel[9];
        kernel[0] = 1.0/16.0; kernel[1] = 2.0/16.0; kernel[2] = 1.0/16.0;
        kernel[3] = 2.0/16.0; kernel[4] = 4.0/16.0; kernel[5] = 2.0/16.0;
        kernel[6] = 1.0/16.0; kernel[7] = 2.0/16.0; kernel[8] = 1.0/16.0;

        vec2 texelSize = 1.0 / texSize * (u_blur * 4.0 + 1.0);

        vec3 sum = vec3(0.0);
        for (int i = 0; i < 9; i++) {
            float y = texture(textureY, vTexCoord + offset[i] * texelSize).r;
            float u = texture(textureU, vTexCoord + offset[i] * texelSize).r - 0.5;
            float v = texture(textureV, vTexCoord + offset[i] * texelSize).r - 0.5;
            sum += yuvToRgb(y, u, v) * kernel[i];
        }
        return sum;
    }

    float sobelEdgeDetection() {
        vec2 texelSize = 1.0 / texSize;

        float tl = texture(textureY, vTexCoord + vec2(-texelSize.x, -texelSize.y)).r;
        float t  = texture(textureY, vTexCoord + vec2(0.0, -texelSize.y)).r;
        float tr = texture(textureY, vTexCoord + vec2(texelSize.x, -texelSize.y)).r;
        float l  = texture(textureY, vTexCoord + vec2(-texelSize.x, 0.0)).r;
        float r  = texture(textureY, vTexCoord + vec2(texelSize.x, 0.0)).r;
        float bl = texture(textureY, vTexCoord + vec2(-texelSize.x, texelSize.y)).r;
        float b  = texture(textureY, vTexCoord + vec2(0.0, texelSize.y)).r;
        float br = texture(textureY, vTexCoord + vec2(texelSize.x, texelSize.y)).r;

        float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
        float gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
        return sqrt(gx*gx + gy*gy);
    }

    vec3 sharpen() {
        vec2 texelSize = 1.0 / texSize;
        vec3 center = getPixelColor();
        vec3 top    = yuvToRgb(texture(textureY, vTexCoord + vec2(0.0, -texelSize.y)).r,
                               texture(textureU, vTexCoord + vec2(0.0, -texelSize.y)).r - 0.5,
                               texture(textureV, vTexCoord + vec2(0.0, -texelSize.y)).r - 0.5);
        vec3 bottom = yuvToRgb(texture(textureY, vTexCoord + vec2(0.0, texelSize.y)).r,
                               texture(textureU, vTexCoord + vec2(0.0, texelSize.y)).r - 0.5,
                               texture(textureV, vTexCoord + vec2(0.0, texelSize.y)).r - 0.5);
        vec3 left   = yuvToRgb(texture(textureY, vTexCoord + vec2(-texelSize.x, 0.0)).r,
                               texture(textureU, vTexCoord + vec2(-texelSize.x, 0.0)).r - 0.5,
                               texture(textureV, vTexCoord + vec2(-texelSize.x, 0.0)).r - 0.5);
        vec3 right  = yuvToRgb(texture(textureY, vTexCoord + vec2(texelSize.x, 0.0)).r,
                               texture(textureU, vTexCoord + vec2(texelSize.x, 0.0)).r - 0.5,
                               texture(textureV, vTexCoord + vec2(texelSize.x, 0.0)).r - 0.5);
        return center * 5.0 - top - bottom - left - right;
    }

    vec3 oilPaintEffect() {
        vec3 color = gaussianBlur();
        float levels = 8.0;
        color = floor(color * levels) / levels;
        return color;
    }

    vec3 applyBaseAdjustments(vec3 color) {
        color += u_brightness;
        color = (color - 0.5) * (1.0 + u_contrast) + 0.5;
        float gray = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(vec3(gray), color, 1.0 + u_saturation);
        return clamp(color, 0.0, 1.0);
    }

    void main()
    {
        vec3 color;

        switch (u_filterType) {
            case 0:
                color = getPixelColor();
                if (u_blur > 0.01) { color = mix(color, gaussianBlur(), u_blur); }
                break;
            case 1:
                color = getPixelColor();
                color.r *= 1.1; color.b *= 0.9;
                float gray = dot(color, vec3(0.299, 0.587, 0.114));
                color = mix(color, vec3(gray), 0.3);
                vec2 center = vTexCoord - 0.5;
                float vignette = 1.0 - dot(center, center) * 0.8;
                color *= vignette;
                color += (fract(sin(dot(vTexCoord, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * 0.05;
                break;
            case 2: color = gaussianBlur(); break;
            case 3:
                color = getPixelColor();
                float edge = sobelEdgeDetection();
                if (edge > 0.3) { color = vec3(0.0); }
                else { float levels = 6.0; color = floor(color * levels) / levels; }
                break;
            case 4: color = getPixelColor(); float bwGray = dot(color, vec3(0.299, 0.587, 0.114)); color = vec3(bwGray); break;
            case 5:
                color = getPixelColor();
                float sepiaGray = dot(color, vec3(0.299, 0.587, 0.114));
                color.r = sepiaGray * 1.2; color.g = sepiaGray * 0.9; color.b = sepiaGray * 0.6;
                color = clamp(color, 0.0, 1.0);
                break;
            case 6: float edgeVal = sobelEdgeDetection(); color = vec3(1.0 - smoothstep(0.1, 0.5, edgeVal)); break;
            case 7: color = sharpen(); break;
            case 8: color = oilPaintEffect(); break;
            case 9:
                color = gaussianBlur();
                float wcGray = dot(color, vec3(0.299, 0.587, 0.114));
                color = mix(vec3(wcGray), color, 1.3);
                float wcLevels = 10.0; color = floor(color * wcLevels) / wcLevels;
                color = clamp(color, 0.0, 1.0);
                break;
            default: color = getPixelColor(); break;
        }

        color = applyBaseAdjustments(color);
        FragColor = vec4(color, 1.0);
    }
)";

VideoGLWidget::VideoGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_ebo(QOpenGLBuffer::IndexBuffer)
{
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    // 初始化 ONNX 超分引擎
    m_srInitialized = m_sr.initialize();
    if (m_srInitialized) {
        qDebug() << "VideoGLWidget: 超分引擎初始化成功";
    } else {
        qDebug() << "VideoGLWidget: 超分引擎初始化失败";
    }

    // 检测显示器分辨率并传递给超分引擎（自动探测）
    updateDisplaySize();
}

VideoGLWidget::~VideoGLWidget()
{
    makeCurrent();

    if (m_textureY) glDeleteTextures(1, &m_textureY);
    if (m_textureU) glDeleteTextures(1, &m_textureU);
    if (m_textureV) glDeleteTextures(1, &m_textureV);

    delete m_program;

    m_currentFrame.release();
    clearFrames();

    doneCurrent();
}

void VideoGLWidget::updateDisplaySize()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize screenSize = screen->size();
        m_sr.setDisplaySize(screenSize.width(), screenSize.height());
        qDebug() << "VideoGLWidget: 显示器分辨率" << screenSize.width() << "x" << screenSize.height()
                 << "已同步到超分引擎";
    }
}

int VideoGLWidget::presentFrame(const Frame *vp)
{
    if (!vp || !vp->frame) return -1;

    AVFrame *newFrame = av_frame_alloc();
    if (!newFrame) return -1;

    if (av_frame_ref(newFrame, vp->frame) < 0) {
        av_frame_free(&newFrame);
        return -1;
    }

    // ===== AI 超分推理 =====
    if (m_srEnabled && m_srInitialized) {
        bool srApplied = m_sr.processFrame(newFrame);
        if (srApplied && m_sr.outputReady()) {
            int srW = m_sr.outputWidth();
            int srH = m_sr.outputHeight();

            AVFrame *srFrame = av_frame_alloc();
            srFrame->format = AV_PIX_FMT_YUV420P;
            srFrame->width = srW;
            srFrame->height = srH;
            av_frame_get_buffer(srFrame, 0);

            // 拷贝 YUV420P 数据（超分输出已经是 planar YUV420P）
            const std::vector<uint8_t> &yuv = m_sr.outputYuvBuffer();
            int ySize = srW * srH;
            int uvSize = ySize / 4;

            // Y plane
            for (int row = 0; row < srH; row++) {
                memcpy(srFrame->data[0] + row * srFrame->linesize[0],
                       yuv.data() + row * srW, srW);
            }
            // U plane
            for (int row = 0; row < srH / 2; row++) {
                memcpy(srFrame->data[1] + row * srFrame->linesize[1],
                       yuv.data() + ySize + row * (srW / 2), srW / 2);
            }
            // V plane
            for (int row = 0; row < srH / 2; row++) {
                memcpy(srFrame->data[2] + row * srFrame->linesize[2],
                       yuv.data() + ySize + uvSize + row * (srW / 2), srW / 2);
            }

            srFrame->pts = newFrame->pts;
            srFrame->pkt_dts = newFrame->pkt_dts;
            srFrame->pict_type = AV_PICTURE_TYPE_I;

            av_frame_unref(newFrame);
            av_frame_free(&newFrame);
            newFrame = srFrame;
        }
        // 如果 srApplied 为 false，可能是源≥显示区域跳过，或上一帧还在推理
        // 此时保留原始帧
    }

    QMutexLocker locker(&m_frameMutex);

    while (m_frameQueue.size() >= MAX_FRAME_QUEUE_SIZE) {
        VideoFrame oldFrame = m_frameQueue.dequeue();
        qDebug() << "Drop old frame pts:" << oldFrame.pts;
        oldFrame.release();
    }

    m_frameQueue.enqueue(VideoFrame(newFrame, vp->pts, vp->duration));

    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

    return 0;
}

void VideoGLWidget::setKeepAspectRatio(bool keep)
{
    m_keepAspectRatio = keep;
    update();
}

void VideoGLWidget::clearFrames()
{
    QMutexLocker locker(&m_frameMutex);
    while (!m_frameQueue.isEmpty()) {
        VideoFrame frame = m_frameQueue.dequeue();
        frame.release();
    }
}

void VideoGLWidget::setFilterParams(float brightness, float contrast, float saturation, float blur)
{
    m_brightness = brightness;
    m_contrast = contrast;
    m_saturation = saturation;
    m_blur = blur;
    update();
}

void VideoGLWidget::setFilterType(int type)
{
    m_filterType = type;
    update();
}

void VideoGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    initShaders();

    glGenTextures(1, &m_textureY);
    glGenTextures(1, &m_textureU);
    glGenTextures(1, &m_textureV);
}

void VideoGLWidget::initShaders()
{
    m_program = new QOpenGLShaderProgram(this);

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qDebug() << "Vertex shader error:" << m_program->log();
        return;
    }

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qDebug() << "Fragment shader error:" << m_program->log();
        return;
    }

    if (!m_program->link()) {
        qDebug() << "Shader link error:" << m_program->log();
        return;
    }

    float vertices[] = {
        -1.0f,  1.0f,      0.0f, 0.0f,
        -1.0f, -1.0f,      0.0f, 1.0f,
         1.0f, -1.0f,      1.0f, 1.0f,
         1.0f,  1.0f,      1.0f, 0.0f
    };

    unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(indices, sizeof(indices));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();
}

void VideoGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    {
        QMutexLocker locker(&m_frameMutex);
        if (!m_frameQueue.isEmpty()) {
            while (m_frameQueue.size() > 2) {
                VideoFrame skip = m_frameQueue.dequeue();
                qDebug() << "Skip frame pts:" << skip.pts;
                skip.release();
            }

            m_currentFrame.release();
            m_currentFrame = m_frameQueue.dequeue();
            m_needUpdateTexture = true;
        }
    }

    if (!m_currentFrame.frame) return;

    if (m_needUpdateTexture) {
        updateTextures(m_currentFrame.frame);
        m_needUpdateTexture = false;
    }

    m_program->bind();
    m_vao.bind();

    m_program->setUniformValue("transform", m_transformMatrix);

    m_program->setUniformValue("u_brightness", m_brightness);
    m_program->setUniformValue("u_contrast", m_contrast);
    m_program->setUniformValue("u_saturation", m_saturation);
    m_program->setUniformValue("u_blur", m_blur);
    m_program->setUniformValue("u_filterType", m_filterType);
    m_program->setUniformValue("texSize", QVector2D((float)m_frameWidth, (float)m_frameHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureY);
    m_program->setUniformValue("textureY", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textureU);
    m_program->setUniformValue("textureU", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_textureV);
    m_program->setUniformValue("textureV", 2);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    m_vao.release();
    m_program->release();
}

// ============================================================
// 纹理更新（YUV420P AVFrame → OpenGL textures）
// ============================================================

void VideoGLWidget::updateTextures(AVFrame *frame)
{
    if (!frame) return;

    int width = frame->width;
    int height = frame->height;

    if (width != m_frameWidth || height != m_frameHeight) {
        m_frameWidth = width;
        m_frameHeight = height;
        calculateAspectRatioTransform(this->width(), this->height());
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, m_textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

    glBindTexture(GL_TEXTURE_2D, m_textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

    glBindTexture(GL_TEXTURE_2D, m_textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0,
                 GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void VideoGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    calculateAspectRatioTransform(w, h);

    // 将视频渲染区域尺寸传递给超分引擎（用于更精确的预降分辨率决策）
    if (m_srInitialized) {
        m_sr.setRenderAreaSize(w, h);
    }
}

QRect VideoGLWidget::calculateTargetRect()
{
    if (!m_keepAspectRatio || m_frameWidth == 0 || m_frameHeight == 0) return rect();

    qreal videoRatio = (qreal)m_frameWidth / m_frameHeight;
    qreal widgetRatio = (qreal)width() / height();

    int targetWidth, targetHeight;
    if (videoRatio > widgetRatio) {
        targetWidth = width();
        targetHeight = (int)(width() / videoRatio);
    } else {
        targetHeight = height();
        targetWidth = (int)(height() * videoRatio);
    }

    int x = (width() - targetWidth) / 2;
    int y = (height() - targetHeight) / 2;

    return QRect(x, y, targetWidth, targetHeight);
}

void VideoGLWidget::calculateAspectRatioTransform(int w, int h)
{
    const float targetRatio = 16.0f / 9.0f;
    float windowRatio = (float)w / (float)h;

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (windowRatio > targetRatio) {
        scaleX = targetRatio / windowRatio;
    } else {
        scaleY = windowRatio / targetRatio;
    }

    m_transformMatrix.setToIdentity();
    m_transformMatrix.scale(scaleX, scaleY, 1.0f);
}

// ============================================================
// AI 超分相关方法
// ============================================================

void VideoGLWidget::setSREnabled(bool enabled)
{
    m_srEnabled = enabled;
    m_sr.setEnabled(enabled);
    qDebug() << "VideoGLWidget: 超分" << (enabled ? "已启用" : "已禁用");
}

bool VideoGLWidget::loadSRModel(int modelIndex)
{
    if (!m_srInitialized) {
        qDebug() << "VideoGLWidget: 超分引擎未初始化，无法加载模型";
        return false;
    }

    bool ok = m_sr.loadModel(modelIndex);
    if (ok) {
        qDebug() << "VideoGLWidget: 模型加载成功，索引=" << modelIndex;
    } else {
        qCritical() << "VideoGLWidget: 模型加载失败，索引=" << modelIndex;
        m_srEnabled = false;
    }
    return ok;
}

int VideoGLWidget::currentSRModel() const
{
    return m_sr.currentModelIndex();
}

float VideoGLWidget::lastSRInferenceMs() const
{
    return m_sr.lastInferenceTimeMs();
}
