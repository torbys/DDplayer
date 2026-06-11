#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include "ijkmediaplayer.h"
#include "filtersettings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    int message_loop(void *arg);
    void OnPlayOrPause();
    void OnStop();
    int OutPutVideo(const Frame* frame);

    void OnSeek(double percent);
    void OnSeekFinished(double percent);
private slots:

    void OnUpdateTimer();  // 定时更新控制栏
    void ToggleMaximize();  // 最大化/还原切换
    void OnCloseWindow();  // 自定义关闭，处理资源释放

    void OnPlayFile(const QString &filePath);  // 播放指定文件
    void OnPlayStream(const QString &url);   // 播放网络流

    // 新增：更新进度条（由子线程通过信号触发）
    void OnUpdateProgress(double positionSec);

    // 新增：播放结束处理
    void OnPlaybackFinished();

private:

    // ===== 键盘事件：倍速 & 单击左右 seek =====
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void setupPlayList();  // 初始化播放列表连接
    void releasePlayer();  // 释放播放器资源

    Ui::MainWindow *ui;
    IjkMediaPlayer *mp_ = NULL;    //中间层
    QTimer *updateTimer_ = nullptr;  // 更新定时器
    FilterSettings *SettingWindow = nullptr;


    bool pause = false;
    QString currentFilePath_;  // 当前播放的文件路径
    bool isPlaying_ = false;

    double currentTime = 0;

    //用于跨线程信号传递
    int64_t totalTime_ = 0;      // 总时长（秒）
    bool isPlaybackFinished_ = false;

    // ===== 倍速 & 方向键 seek 相关 =====
    // 长按右方向键期间是否正在 2x 快速播放
    bool isSpeedUp_ = false;
    // 用于区分单击和长按：keyPressEvent 首次触发时记录，如果 isAutoRepeat 则不做 seek
    bool rightKeyHeld_ = false;   // 右方向键是否已经进入长按状态
    bool leftKeyHeld_  = false;   // 左方向键是否已经进入长按状态（可选，同样处理）

    // seek 步长（秒）
    static constexpr int64_t kSeekStepSec = 10;


};
#endif // MAINWINDOW_H
