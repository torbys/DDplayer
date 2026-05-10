#include "DisplayVideo.h"
#include "ui_DisplayVideo.h"
#include "VideoGLWidget.h"
#include "ffplay_def.h"

DisplayVideo::DisplayVideo(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DisplayVideo)
{
    ui->setupUi(this);
    setupVideoWidget();
}

DisplayVideo::~DisplayVideo()
{
    delete ui;
}

void DisplayVideo::setupVideoWidget()
{
    // 获取UI中的Videowidget（假设objectName为"Videowidget"）
    // 在Qt Designer中将原有的QWidget提升为VideoGLWidget
    // 或者在这里动态替换

    // 方案：查找现有的widget并替换为VideoGLWidget
    QWidget *oldWidget = ui->Videowidget;  // 假设ui文件中的widget名为Videowidget

    if (oldWidget) {
        // 创建VideoGLWidget，保持相同的几何属性
        m_videoWidget = new VideoGLWidget(this);
        m_videoWidget->setGeometry(oldWidget->geometry());
        m_videoWidget->setObjectName(oldWidget->objectName());

        // 替换布局中的widget（如果在布局中）
        QLayout *layout = oldWidget->parentWidget()->layout();
        if (layout) {
            // 找到oldWidget在布局中的位置并替换
            for (int i = 0; i < layout->count(); ++i) {
                if (layout->itemAt(i)->widget() == oldWidget) {
                    layout->replaceWidget(oldWidget, m_videoWidget);
                    break;
                }
            }
        }

        // 删除旧的widget
        oldWidget->hide();
        oldWidget->deleteLater();

        // 确保VideoGLWidget可见
        m_videoWidget->show();
    } else {
        // 如果没找到，创建一个全屏的
        m_videoWidget = new VideoGLWidget(this);
        m_videoWidget->setGeometry(rect());
        m_videoWidget->show();
    }
}

int DisplayVideo::drawFrame(const Frame *frame)
{
    if (!m_videoWidget) {
        return -1;
    }

    // 将帧传递给OpenGL控件渲染
    // 这个函数会被FFPlayer的video_refresh_callback_回调调用
    // 可能在非主线程，所以presentFrame内部做了线程安全处理
    return m_videoWidget->presentFrame(frame);
}

void DisplayVideo::clearVideo()
{
    if (m_videoWidget) {
        m_videoWidget->clearFrames();
        m_videoWidget->update();
    }
}
