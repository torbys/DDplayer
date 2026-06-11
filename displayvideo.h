#ifndef DISPLAYVIDEO_H
#define DISPLAYVIDEO_H

#include <QWidget>
#include "videoglwidget.h"

namespace Ui {
class DisplayVideo;
}

class DisplayVideo : public QWidget
{
    Q_OBJECT

public:
    explicit DisplayVideo(QWidget *parent = nullptr);
    ~DisplayVideo();

    // 视频渲染接口，供FFPlayer回调调用
    // 返回值：0成功，-1失败
    int drawFrame(const Frame *frame);

    // 清除视频画面
    void clearVideo();

    // 获取 VideoGLWidget 指针（供外部连接信号）
    VideoGLWidget* getVideoWidget() const { return m_videoWidget; }

private:
    Ui::DisplayVideo *ui;

    // OpenGL视频渲染控件（提升后的Videowidget）
    VideoGLWidget *m_videoWidget = nullptr;

    // 初始化视频显示控件
    void setupVideoWidget();
};

#endif // DISPLAYVIDEO_H
