#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include <QInputDialog>
#include <QLineEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

CtrlBar::CtrlBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CtrlBar)
{
    ui->setupUi(this);

    connect(ui->VioceSlider, &QSlider::valueChanged, this, &CtrlBar::on_VioceSlider_valueChanged);
    //connect(ui->NetBtn, &QPushButton::clicked, this, &CtrlBar::on_NetBtn_clicked);
}

CtrlBar::~CtrlBar()
{
    delete ui;
}

void CtrlBar::init_CtrlBar(int64_t Duration)
{
    qDebug() << "init_CtrlBar Total time:" << Duration << "seconds";

    totalTime_ = Duration;
    currentTime_ = 0;
    isSeeking_ = false;

    // 初始化视频滑块（范围：0 ~ 总秒数）
    ui->VideoSlider->setMinimum(0);
    ui->VideoSlider->setMaximum(static_cast<int>(Duration));
    ui->VideoSlider->setValue(0);
    ui->VideoSlider->setSingleStep(1);

    // 初始化音量滑块（0 ~ 150，默认75即50%）
    ui->VioceSlider->setRange(0, 150);
    ui->VioceSlider->setValue(75);  // 默认音量50%（75/150）

    // 使用 QTime 构造函数 (时, 分, 秒, 毫秒)
    int h = Duration / 3600;
    int m = (Duration % 3600) / 60;
    int s = Duration % 60;
    ui->timeEdit_total->setTime(QTime(h, m, s));

    // 设置总时间显示
    ui->timeEdit_total->setDisplayFormat("HH:mm:ss");

    // 设置当前时间显示为 00:00:00
    ui->timeEdit_Now->setTime(QTime(0, 0, 0));
    ui->timeEdit_Now->setDisplayFormat("HH:mm:ss");

}

void CtrlBar::Set_Video_Time(int64_t now)
{
    int64_t i = Total_time / now;
    int64_t value = i * 200;

    ui->VideoSlider->setValue(value);
}

void CtrlBar::SetCurrentTime(int64_t seconds)
{
    // 如果用户正在拖动滑块，不更新（避免冲突）
    if (isSeeking_) {
        return;
    }

    // 限制范围
    if (seconds < 0) seconds = 0;
    if (seconds > totalTime_) seconds = totalTime_;

    currentTime_ = seconds;
    UpdateDisplay(currentTime_);
}

void CtrlBar::on_PlayBtn_clicked()
{
    qDebug()<<"PlayAndPause Clicked";




    emit SigPlayOrPause();

}

void CtrlBar::on_StopBtn_clicked()
{
    qDebug()<<"StopBtn Clicked";

    emit SigOnStop();

}

// 获取当前时间
int64_t CtrlBar::GetCurrentTime() const
{
    return currentTime_;
}

// 获取总时间
int64_t CtrlBar::GetTotalTime() const
{
    return totalTime_;
}

void CtrlBar::setPauseState(bool pause)
{
    is_pause = pause;

    if(!is_pause){
        ui->PlayBtn->setStyleSheet(R"(
            QPushButton {
                border: none;
                background: transparent;
                color: #cccccc;
                image: url(:/media/icon/Pause.png);
            }

            QPushButton:hover {
                background: transparent;
                color: #ffffff;
                border: none;
                padding:3px;
                image: url(:/media/icon/PauseSelected.png);
            }

            QPushButton:pressed {
                background: transparent;

            }
        )");
    }else{
        ui->PlayBtn->setStyleSheet(R"(
            QPushButton {
                border: none;
                background: transparent;
                color: #cccccc;
                padding-left:8px;
                image: url(:/media/icon/Play.png);
            }

            QPushButton:hover {
                background: transparent;
                color: #ffffff;
                border: none;
                padding-left:8px;
                image: url(:/media/icon/PlaySelected.png);
            }

            QPushButton:pressed {
                background: transparent;
            }
        )");
    }


}

void CtrlBar::setSpeed(double value)
{

    ui->SpeedBtn->setText(QString("倍速 × %1").arg(value));;

    speed = value;

}

void CtrlBar::UpdateDisplay(int64_t seconds)
{
    // 更新时间显示
    ui->timeEdit_Now->setTime(SecondsToQTime(seconds));

    // 更新滑块位置（阻塞信号避免循环触发）
    ui->VideoSlider->blockSignals(true);
    ui->VideoSlider->setValue(static_cast<int>(seconds));
    ui->VideoSlider->blockSignals(false);
}

QTime CtrlBar::SecondsToQTime(int64_t seconds) const
{
    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>((seconds % 3600) / 60);
    int s = static_cast<int>(seconds % 60);
    return QTime(h, m, s);
}


void CtrlBar::on_VideoSlider_valueChanged(int value)
{
    // 只有在用户主动操作时（isSeeking_ 为 true）才处理
    if (!isSeeking_) {
        return;
    }

    currentTime_ = value;

    // 更新时间显示（实时反馈）
    ui->timeEdit_Now->setTime(SecondsToQTime(currentTime_));

    // 计算百分比并发送 seek 信号（实时预览）
    double percent = (totalTime_ > 0) ? (static_cast<double>(value) / totalTime_) : 0.0;
    emit SigSeek(percent);

    qDebug() << "Seeking to:" << value << "seconds, percent:" << percent;
}


void CtrlBar::on_VideoSlider_sliderPressed()
{
    isSeeking_ = true;
    qDebug() << "Slider pressed, seeking started";
}



void CtrlBar::on_VideoSlider_sliderReleased()
{
    int value = ui->VideoSlider->value();
    currentTime_ = value;

    // 计算百分比
    double percent = (totalTime_ > 0) ? (static_cast<double>(value) / totalTime_) : 0.0;

    qDebug() << "Slider released, seek finished at:" << value << "seconds, percent:" << percent;

    // 发送 seek 完成信号
    emit SigSeekFinished(percent);

    isSeeking_ = false;
}

void CtrlBar::on_VioceSlider_valueChanged(int value)
{
    qDebug()<<"voice value:"<<value;

    if (isMuted_ && value > 0) {
        isMuted_ = false;
        ui->vioce->setIcon(QIcon(":/media/icon/VolumeLoud.png"));
    }
    emit SigVolumeChanged(value);
}

void CtrlBar::on_vioce_clicked()
{
    isMuted_ = !isMuted_;

    if (isMuted_) {
        ui->vioce->setIcon(QIcon(":/media/icon/VolumeCross.png"));
        ui->VioceSlider->blockSignals(true);
        lastVolume_ = ui->VioceSlider->value();
        ui->VioceSlider->setValue(0);
        ui->VioceSlider->blockSignals(false);
        emit SigMuteToggled(true);
        emit SigVolumeChanged(0);
    } else {
        ui->vioce->setIcon(QIcon(":/media/icon/VolumeLoud.png"));
        ui->VioceSlider->blockSignals(true);
        ui->VioceSlider->setValue(lastVolume_);
        ui->VioceSlider->blockSignals(false);
        emit SigMuteToggled(false);
    }
}


void CtrlBar::on_PreBtn_clicked()
{
    emit SigPreVideo();
}

void CtrlBar::on_NextBtn_clicked()
{
    emit SigNextVideo();
}

void CtrlBar::setNavButtonState(bool canPre, bool canNext)
{
    ui->PreBtn->setEnabled(canPre);
    ui->NextBtn->setEnabled(canNext);
}

void CtrlBar::on_NetBtn_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("打开网络流");
    dialog.setFixedSize(450, 180);
    dialog.setStyleSheet(R"(
        QDialog {
            background-color: #2b2b2b;
            color: #ffffff;
            border-radius: 8px;
        }
        QLabel {
            color: #cccccc;
            font-size: 13px;
        }
        QLineEdit {
            background-color: #3a3a3a;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 8px 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #0078d4;
        }
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 8px 24px;
            color: #ffffff;
            font-size: 13px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #666666;
        }
        QPushButton#btnPlay {
            background-color: #0078d4;
            border-color: #0078d4;
        }
        QPushButton#btnPlay:hover {
            background-color: #1084d8;
        }
    )");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 16);

    QLabel *label = new QLabel("请输入流媒体地址：", &dialog);
    layout->addWidget(label);

    QLineEdit *lineEdit = new QLineEdit(&dialog);
    lineEdit->setPlaceholderText("rtmp:// / http:// / rtsp:// ...");

    QSettings settings("DDplayer", "StreamHistory");
    QString lastUrl = settings.value("lastUrl", "").toString();
    if (!lastUrl.isEmpty()) {
        lineEdit->setText(lastUrl);
    }
    layout->addWidget(lineEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    QPushButton *playBtn = new QPushButton("播放", &dialog);
    playBtn->setObjectName("btnPlay");
    playBtn->setDefault(true);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(playBtn);
    layout->addLayout(btnLayout);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(playBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    lineEdit->setFocus();

    if (dialog.exec() == QDialog::Accepted) {
        QString url = lineEdit->text().trimmed();
        if (!url.isEmpty()) {
            settings.setValue("lastUrl", url);
            qDebug() << "NetBtn: 播放网络流:" << url;
            emit SigPlayStream(url);
        }
    }
}

void CtrlBar::on_PlayListBtn_clicked()
{
    ListShow = !ListShow;

    emit SigShowList(ListShow);
}

void CtrlBar::on_SettingBtn_clicked()
{
    emit SigSettingOpen();
}

