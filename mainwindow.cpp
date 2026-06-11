#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QCheckBox>
#include <QComboBox>

#include <onnxruntime_cxx_api.h>
#include <onnxruntime_run_options_config_keys.h>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ========== 1. 去除系统边框 ==========
    setWindowFlags(Qt::FramelessWindowHint);

    QWidget* titleBar1 = new QWidget();
    ui->titleDockWidget->setTitleBarWidget(titleBar1);

    QWidget* titleBar2 = new QWidget();
    ui->listDockWidget->setTitleBarWidget(titleBar2);

    // ========== 2. 连接 TitleBar 按钮信号 ==========
    connect(ui->titleContents, &TitleBar::SigMinimize, this, &MainWindow::showMinimized);
    connect(ui->titleContents, &TitleBar::SigMaximize, this, &MainWindow::ToggleMaximize);
    connect(ui->titleContents, &TitleBar::SigClose, this, &MainWindow::OnCloseWindow);

    // ========== 3. 连接拖动信号 ==========
    connect(ui->titleContents, &TitleBar::SigDragMove, this, [=](const QPoint &pos){
        if (!isMaximized()) {
            move(pos);
        }
    });

    // ========== 4. 连接控制栏信号 ==========
    connect(ui->CtrlBarwidget, &CtrlBar::SigPlayOrPause, this, &MainWindow::OnPlayOrPause);
    connect(ui->CtrlBarwidget, &CtrlBar::SigOnStop, this, &MainWindow::OnStop);
    connect(ui->CtrlBarwidget, &CtrlBar::SigSeek, this, &MainWindow::OnSeek);
    connect(ui->CtrlBarwidget, &CtrlBar::SigSeekFinished, this, &MainWindow::OnSeekFinished);
    connect(ui->CtrlBarwidget, &CtrlBar::SigVolumeChanged, this, [this](int volume) {
        if (mp_) {
            mp_->ijkmp_set_volume(static_cast<float>(volume));
        }
    });
    connect(ui->CtrlBarwidget, &CtrlBar::SigMuteToggled, this, [this](bool muted) {
        if (mp_) {
            mp_->ijkmp_set_mute(muted);
        }
    });
    connect(ui->CtrlBarwidget,&CtrlBar::SigShowList,this,[this](bool showList){

        if(showList){
            ui->listDockWidget->show();
        }else{
            ui->listDockWidget->hide();
        }

    });

    // ========== 5. 连接播放列表信号 ==========
    setupPlayList();

    // ========== 6. 连接上一个/下一个视频 ==========
    connect(ui->CtrlBarwidget, &CtrlBar::SigPreVideo, ui->listContents, &PlayList::playPrevious);
    connect(ui->CtrlBarwidget, &CtrlBar::SigNextVideo, ui->listContents, &PlayList::playNext);
    connect(ui->listContents, &PlayList::sigNavButtonStateChanged, ui->CtrlBarwidget, &CtrlBar::setNavButtonState);
    connect(ui->CtrlBarwidget, &CtrlBar::SigPlayStream, this, &MainWindow::OnPlayStream);
    connect(ui->CtrlBarwidget,&CtrlBar::SigSettingOpen,this,[this](){
        if(!SettingWindow){
            SettingWindow = new FilterSettings(nullptr);
            SettingWindow->setAttribute(Qt::WA_DeleteOnClose);

            connect(SettingWindow, &QWidget::destroyed, this, [this](){
                SettingWindow = nullptr;
            });

            // 连接滤镜参数变化信号到 VideoGLWidget
            VideoGLWidget *videoWidget = ui->showVideowidget->getVideoWidget();
            if (videoWidget) {
                connect(SettingWindow, &FilterSettings::filterParamsChanged,
                        videoWidget, &VideoGLWidget::setFilterParams);
                connect(SettingWindow, &FilterSettings::filterTypeChanged,
                        videoWidget, &VideoGLWidget::setFilterType);

                // 连接 AI 超分信号到 VideoGLWidget
                connect(SettingWindow, &FilterSettings::srEnabledChanged,
                        videoWidget, &VideoGLWidget::setSREnabled);
                connect(SettingWindow, &FilterSettings::srModelChanged,
                        videoWidget, &VideoGLWidget::loadSRModel);

                // 连接信号后，手动触发一次超分状态同步（因为 loadSettings() 在连接前已执行）
                QCheckBox *srCB = SettingWindow->findChild<QCheckBox*>("srCheckBox");
                QComboBox *srCombo = SettingWindow->findChild<QComboBox*>("srModelCombo");
                if (srCB && srCB->isChecked() && srCombo) {
                    qDebug() << "MainWindow: 同步超分状态到 VideoGLWidget";
                    videoWidget->setSREnabled(true);
                    videoWidget->loadSRModel(srCombo->currentIndex());
                }
            }

            SettingWindow->show();
        } else {
            SettingWindow->raise();
            SettingWindow->activateWindow();
        }
    });

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupPlayList()
{
    // 连接播放列表的信号
    connect(ui->listContents, &PlayList::sigPlayFile, this, &MainWindow::OnPlayFile);

    // 如果需要处理添加文件的其他逻辑，可以连接sigAddFile
    connect(ui->listContents, &PlayList::sigAddFile, this, [this](const QString &path) {
        qDebug() << "添加文件到播放列表:" << path;
    });

    // 播放列表此时已经加载了设置，如果有历史文件，设置第一个为当前文件
    QString firstFile = ui->listContents->currentFilePath();
    if (!firstFile.isEmpty()) {
        currentFilePath_ = firstFile;

        // 更新标题栏显示文件名（不带扩展名）
        QFileInfo fileInfo(firstFile);
        ui->titleContents->initTitle(fileInfo.completeBaseName());

        qDebug() << "自动设置当前文件为:" << firstFile;
    }

}

// 播放指定文件（从播放列表调用）
void MainWindow::OnPlayFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    QString videoName = fileInfo.completeBaseName();

    ui->titleContents->initTitle(videoName);

    if (mp_) {
        releasePlayer();
    }

    currentFilePath_ = filePath;

    //currentFilePath_ = "rtmp://127.0.0.1:1935/live/test";

    isPlaybackFinished_ = false;
    isPlaying_ = false;

    mp_ = new IjkMediaPlayer();

    int ret = mp_->ijkmp_create(std::bind(&MainWindow::message_loop, this, std::placeholders::_1));
    if (ret < 0) {
        qDebug() << "IjkMediaPlayer create failed";
        delete mp_;
        mp_ = nullptr;
        return;
    }

    mp_->ijkmp_set_data_source(filePath.toUtf8().constData());

    ret = mp_->ijkmp_prepare_async();
    if (ret < 0) {
        qDebug() << "IjkMediaPlayer prepare failed";
        delete mp_;
        mp_ = nullptr;
        return;
    }

    mp_->AddVideoRefreshCallback(std::bind(&MainWindow::OutPutVideo, this, std::placeholders::_1));

    mp_->AddProgressCallback([this](double positionSec) {
        QMetaObject::invokeMethod(this, [this, positionSec]() {
            OnUpdateProgress(positionSec);
        }, Qt::QueuedConnection);
    });

    this->setFocus();

    qDebug() << "开始播放:" << filePath;
}

void MainWindow::OnPlayStream(const QString &url)
{
    if (url.isEmpty()) {
        return;
    }

    QUrl qurl(url);
    QString videoName = qurl.host();
    if (videoName.isEmpty()) {
        videoName = url.left(40) + (url.length() > 40 ? "..." : "");
    }
    ui->titleContents->initTitle(videoName);

    if (mp_) {
        releasePlayer();
    }

    currentFilePath_ = url;
    isPlaybackFinished_ = false;
    isPlaying_ = false;

    mp_ = new IjkMediaPlayer();

    int ret = mp_->ijkmp_create(std::bind(&MainWindow::message_loop, this, std::placeholders::_1));
    if (ret < 0) {
        qDebug() << "IjkMediaPlayer create failed";
        delete mp_;
        mp_ = nullptr;
        return;
    }

    mp_->ijkmp_set_data_source(url.toUtf8().constData());

    ret = mp_->ijkmp_prepare_async();
    if (ret < 0) {
        qDebug() << "IjkMediaPlayer prepare failed for stream:" << url;
        delete mp_;
        mp_ = nullptr;
        return;
    }

    mp_->AddVideoRefreshCallback(std::bind(&MainWindow::OutPutVideo, this, std::placeholders::_1));

    mp_->AddProgressCallback([this](double positionSec) {
        QMetaObject::invokeMethod(this, [this, positionSec]() {
            OnUpdateProgress(positionSec);
        }, Qt::QueuedConnection);
    });

    this->setFocus();
    qDebug() << "开始播放网络流:" << url;
}

void MainWindow::OnUpdateProgress(double positionSec)
{
    if (!mp_ || !ui->CtrlBarwidget) {
        return;
    }

    int64_t currentSec = static_cast<int64_t>(positionSec);

    currentTime = positionSec;

    ui->CtrlBarwidget->SetCurrentTime(currentSec);

    if (!isPlaybackFinished_) {
        int64_t totalSec = mp_->ijk_get_Total_Time();
        if (totalSec > 0 && currentSec >= totalSec - 1) {
            isPlaybackFinished_ = true;
            OnPlaybackFinished();
        }
    }
}

void MainWindow::OnPlaybackFinished()
{
    qDebug() << "播放结束！";

    if (mp_ && isPlaying_) {
        mp_->ijk_toggle_pause();
        isPlaying_ = false;

        ui->CtrlBarwidget->setPauseState(true);

        int64_t totalSec = mp_->ijk_get_Total_Time();
        ui->CtrlBarwidget->SetCurrentTime(totalSec);
    }

    isPlaybackFinished_ = true;
}

void MainWindow::releasePlayer()
{
    if (mp_) {
        mp_->ijkmp_stop();
        mp_->ijkmp_destroy();
        delete mp_;
        mp_ = nullptr;
    }

    isPlaying_ = false;
    isPlaybackFinished_ = false;
}

void MainWindow::ToggleMaximize()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::OnCloseWindow()
{
    // ✅ 简化：不再需要手动清理定时器
    this->OnStop();
    close();
}

// 定时更新控制栏（每秒调用）
void MainWindow::OnUpdateTimer()
{
    if (!mp_ || !ui->CtrlBarwidget) return;

    // 从播放器获取当前播放时间（秒）
    // 假设播放器有方法获取当前时间，这里需要根据你的 FFPlayer 实现
    int64_t currentSec = mp_->ijkmp_get_current_position();  // 你需要实现这个方法

    // 更新控制栏显示
    ui->CtrlBarwidget->SetCurrentTime(currentSec);
}

//Seek 信号处理（拖动过程中）
void MainWindow::OnSeek(double percent)
{
    qDebug() << "Seeking to percent:" << percent;
    // 可以在这里做预览，或者什么都不做
}

// Seek 完成（用户释放滑块）
void MainWindow::OnSeekFinished(double percent)
{
    qDebug() << "Seek finished, percent:" << percent;

    if (!mp_) return;

    if (isPlaybackFinished_) {
        qDebug() << "Seek during playback finished, resuming playback";
        if (!isPlaying_) {
            mp_->ijk_toggle_pause();
            isPlaying_ = true;
        }
        isPlaybackFinished_ = false;
        ui->CtrlBarwidget->setPauseState(false);
    }

    int64_t totalTime = mp_->ijk_get_Total_Time();
    int64_t targetTime = static_cast<int64_t>(totalTime * percent);

    mp_->ijkmp_seek_to(targetTime * 1000);
}


int MainWindow::message_loop(void *arg)
{

    IjkMediaPlayer *mp =(IjkMediaPlayer *)arg;

    qDebug() << "message_loop into";
    while (1) {
        AVMessage msg;
        //取消息队列的消息，如果没有消息就阻塞，直到有消息被发到消息队列。
        int retval = mp->ijkmp_get_msg(&msg, 1);

        if (retval < 0)
            break;
        switch (msg.what) {
        case FFP_MSG_FLUSH:
            qDebug() << __FUNCTION__ << " FFP_MSG_FLUSH";
            break;
        case FFP_MSG_PREPARED:
            std::cout << __FUNCTION__ << " FFP_MSG_PREPARED" << std::endl;
            ui->CtrlBarwidget->init_CtrlBar(mp_->ijk_get_Total_Time());

            totalTime_ = mp_->ijk_get_Total_Time();  // ✅ 保存总时长
            isPlaying_ = true;
            isPlaybackFinished_ = false;              // ✅ 重置结束标志

            ui->CtrlBarwidget->setPauseState(false);
            mp->ijkmp_start();
            break;
        case FFP_MSG_COMPLETED:
            qDebug() << "FFP_MSG_RECEIVED: Playback completed";
            if (!isPlaybackFinished_) {
                isPlaybackFinished_ = true;
                QMetaObject::invokeMethod(this, &MainWindow::OnPlaybackFinished, Qt::QueuedConnection);
            }
            break;
        default:
            qDebug() << __FUNCTION__ << " default " << msg.what ;
            break;
        }

        msg_free_res(&msg);

        //    qDebug() << "message_loop sleep, mp:" << mp;
        // 先模拟线程运行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    qDebug() << "message_loop leave";

    return 0;
}

void MainWindow::OnPlayOrPause()
{
    qDebug() << "OnPlayOrPause call";

    if (currentFilePath_.isEmpty() && !mp_) {
        qDebug() << "播放列表为空，请先添加视频文件";
        return;
    }

    if (!mp_ && !currentFilePath_.isEmpty()) {
        OnPlayFile(currentFilePath_);
        return;
    }

    if (mp_) {
        if (isPlaybackFinished_) {
            qDebug() << "播放已完成，从头开始播放";
            mp_->ijkmp_seek_to(0);
            if (!isPlaying_) {
                mp_->ijk_toggle_pause();
                isPlaying_ = true;
            }
            isPlaybackFinished_ = false;
            ui->CtrlBarwidget->setPauseState(false);
            return;
        }

        mp_->ijk_toggle_pause();
        isPlaying_ = !isPlaying_;
        ui->CtrlBarwidget->setPauseState(!isPlaying_);
    }
}

void MainWindow::OnStop()
{
    qDebug()<<"Stop into";
    releasePlayer();
    ui->CtrlBarwidget->setPauseState(true);
}

int MainWindow::OutPutVideo(const Frame* frame)
{
    // 将帧传递给DisplayVideo进行OpenGL渲染
    // 这个函数在FFPlayer::video_refresh_thread线程中被调用
    if (ui->showVideowidget) {

        //Set_Video_Time();

        return ui->showVideowidget->drawFrame(frame);
    }
    return -1;
}

// =============================================================
//  键盘事件处理
//
//  右方向键:
//    - 首次按下（isAutoRepeat == false）→ 记录为"已按下"，等待看是否
//      变成长按（系统 autoRepeat）
//    - isAutoRepeat == true → 确认是长按，启动 2x 快速播放
//    - keyReleaseEvent  → 恢复 1x，并在非长按时执行 +3s seek
//
//  左方向键:
//    - 首次按下且非长按 → +3s（松开时判断）
//    - 长按左方向键不做额外处理（可按需扩展）
// =============================================================
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!mp_ || isPlaybackFinished_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    switch (event->key()) {

    case Qt::Key_Right:
        if (!event->isAutoRepeat()) {
            // 首次按下：还不知道是单击还是长按，先标记
            rightKeyHeld_ = false;
        } else {
            // 系统产生了 autoRepeat，确认是长按
            if (!rightKeyHeld_) {
                rightKeyHeld_ = true;
                isSpeedUp_    = true;
                mp_->ijkmp_set_playback_rate(2.0);
                ui->CtrlBarwidget->setSpeed(2.0);
            }
        }
        event->accept();
        break;

    case Qt::Key_Left:
        if (!event->isAutoRepeat()) {
            leftKeyHeld_ = false;   // 重置，等待松开时判断
        }
        // 左方向键长按暂不做特殊处理，仅 keyRelease 时判断单击
        event->accept();
        break;

    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!mp_ || isPlaybackFinished_) {
        QMainWindow::keyReleaseEvent(event);
        return;
    }

    // autoRepeat 的 release 事件忽略（松开真实按键时 isAutoRepeat 为 false）
    if (event->isAutoRepeat()) {
        QMainWindow::keyReleaseEvent(event);
        return;
    }

    switch (event->key()) {

    case Qt::Key_Right:
        if (isSpeedUp_) {
            // 长按松开 → 恢复正常速率
            isSpeedUp_    = false;
            rightKeyHeld_ = false;
            mp_->ijkmp_set_playback_rate(1.0);
            ui->CtrlBarwidget->setSpeed(1.0);
        } else {
            // 短按（单击）→ 向前 seek 3s
            double totalSec = static_cast<double>(mp_->ijk_get_Total_Time());
            double targetSec = qMin(currentTime + kSeekStepSec, totalSec > 0 ? totalSec - 0.5 : currentTime + kSeekStepSec);
            mp_->ijkmp_seek_to(static_cast<long>(targetSec * 1000.0));
            qDebug() << "单击右方向键：向前 seek" << kSeekStepSec << "s →" << targetSec << "s";
        }
        rightKeyHeld_ = false;
        event->accept();
        break;

    case Qt::Key_Left:
        if (!leftKeyHeld_) {
            // 短按（单击）→ 向后 seek 3s
            double targetSec = qMax(currentTime - kSeekStepSec, 0.0);
            mp_->ijkmp_seek_to(static_cast<long>(targetSec * 1000.0));
            qDebug() << "单击左方向键：向后 seek" << kSeekStepSec << "s →" << targetSec << "s";
        }
        leftKeyHeld_ = false;
        event->accept();
        break;

    case Qt::Key_Space:
        OnPlayOrPause();

    default:
        QMainWindow::keyReleaseEvent(event);
        break;
    }
}
