#ifndef CTRLBAR_H
#define CTRLBAR_H

#include <QWidget>

namespace Ui {
class CtrlBar;
}

class CtrlBar : public QWidget
{
    Q_OBJECT

public:
    explicit CtrlBar(QWidget *parent = nullptr);
    ~CtrlBar();

    void init_CtrlBar(int64_t Duration);
    void Set_Video_Time(int64_t now);

    // 设置当前播放时间（秒），由外部定时调用
    void SetCurrentTime(int64_t seconds);

    int64_t GetCurrentTime() const;

    int64_t GetTotalTime() const;

    void setPauseState(bool pause);

    void setSpeed(double value);

    void setNavButtonState(bool canPre, bool canNext);

signals:
    void SigPlayOrPause();
    void SigOnStop();
    // 新增：seek 信号，参数为百分比（0.0 ~ 1.0）
    void SigSeek(double percent);
    // 新增：seek 完成信号（用户释放滑块）
    void SigSeekFinished(double percent);
    // 新增：音量变化信号（0~100）
    void SigVolumeChanged(int volume);
    // 新增：静音切换信号
    void SigMuteToggled(bool muted);
    //隐藏或显示视频列表
    void SigShowList(bool show);

    // 新增：上一个/下一个视频
    void SigPreVideo();
    void SigNextVideo();
    // 新增：网络流播放
    void SigPlayStream(const QString &url);
    //新开窗口
    void SigSettingOpen();

private slots:
    void on_PlayBtn_clicked();

    void on_StopBtn_clicked();

    void on_VideoSlider_valueChanged(int value);

    void on_VideoSlider_sliderPressed();

    void on_VideoSlider_sliderReleased();

    void on_VioceSlider_valueChanged(int value);
    void on_vioce_clicked();

    void on_NextBtn_clicked();

    void on_PlayListBtn_clicked();

    void on_PreBtn_clicked();
    void on_NetBtn_clicked();

    void on_SettingBtn_clicked();

private:

    void UpdateDisplay(int64_t seconds);
    QTime SecondsToQTime(int64_t seconds) const;

    int Total_time;
    Ui::CtrlBar *ui;
    bool is_pause = true;
    int lastVolume_ = 75;

    int64_t totalTime_;      // 总时长（秒）
    int64_t currentTime_;    // 当前时间（秒）
    bool isPaused_;          // 暂停状态
    bool isSeeking_;         // 是否正在拖动滑块（避免拖动时冲突）
    bool isMuted_ = false;   // 是否静音
    bool ListShow = true;    // 是否显示列表

    double speed = 0.0;

};

#endif // CTRLBAR_H
