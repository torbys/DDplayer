#include "ijkmediaplayer.h"


IjkMediaPlayer::IjkMediaPlayer()
{

}

IjkMediaPlayer::~IjkMediaPlayer()
{

}


int IjkMediaPlayer::ijkmp_create(std::function<int (void *)> msg_loop)
{
    int ret = 0;
    ffplayer_ = new FFPlayer();

    if(!ffplayer_) {
        std::cout << " new FFPlayer() failed\n ";
        return -1;
    }

    msg_loop_ = msg_loop;

    ret = ffplayer_->ffp_create();

    if(ret < 0) {
        return -1;
    }
    return 0;
}

int IjkMediaPlayer::ijkmp_destroy()
{
    if (msg_thread_ && msg_thread_->joinable()) {
        msg_thread_->join();
        delete msg_thread_;
        msg_thread_ = nullptr;
    }

    if (ffplayer_) {
        ffplayer_->ffp_destroy();
        delete ffplayer_;
        ffplayer_ = nullptr;
    }

    if (data_source_) {
        free(data_source_);
        data_source_ = nullptr;
    }

    return 0;
}

int IjkMediaPlayer::ijkmp_set_data_source(const char *url)
{
    if(!url) {
        return -1;
    }
    data_source_ = strdup(url); // 分配内存+ 拷贝字符串
    return 0;
}

int IjkMediaPlayer::ijkmp_prepare_async()
{
    //将状态设置为异步准备中
    mp_state_ = MP_STATE_ASYNC_PREPARING;

    // 启用消息队列
    msg_queue_start(&ffplayer_->msg_queue_);

    // 创建循环线程
    msg_thread_ = new std::thread(&IjkMediaPlayer::ijkmp_msg_loop, this, this);

    // 调用ffplayer
    int ret = ffplayer_->ffp_prepare_async_l(data_source_);
    if(ret < 0) {
        mp_state_ = MP_STATE_ERROR;
        return -1;
    }
    return 0;

}

int IjkMediaPlayer::ijkmp_start()
{
    ffp_notify_msg1(ffplayer_,FFP_REQ_START);
    return 0;
}

int IjkMediaPlayer::ijkmp_stop()
{
    int ret = ffplayer_->ffp_stop_l();
    if(ret < 0){
        return ret;
    }

    return 0;
}

int IjkMediaPlayer::ijkmp_seek_to(long msec)
{
    if (!ffplayer_) {
        return -1;
    }
    return ffplayer_->ffp_seek_to_l(static_cast<int64_t>(msec));
}

long IjkMediaPlayer::ijkmp_get_current_position()
{
    return static_cast<long>(ffplayer_->get_master_clock());
}

int IjkMediaPlayer::ijkmp_get_msg(AVMessage *msg, int block)
{
    while (1) {
        int continue_wait_next_msg = 0;
        //取消息，如果没有消息则阻塞。
        int retval = msg_queue_get(&ffplayer_->msg_queue_, msg, block);
        if (retval <= 0)    // -1 abort, 0 没有消息
            return retval;

        switch (msg->what) {
        case FFP_MSG_PREPARED:
            std::cout << __FUNCTION__ << " FFP_MSG_PREPARED" << std::endl;
            mp_state_ = MP_STATE_PREPARED;
            break;
        case FFP_REQ_START:
            std::cout << __FUNCTION__ << " FFP_REQ_START" << std::endl;
            continue_wait_next_msg = 1;
            break;
        default:
            std::cout << __FUNCTION__ << " default " << msg->what << std::endl;
            break;
        }

        if (continue_wait_next_msg) {
            msg_free_res(msg);
            continue;
        }

        return retval;
    }

    return -1;
}

void IjkMediaPlayer::AddVideoRefreshCallback(std::function<int (const Frame *)> callback)
{
    ffplayer_->AddVideoRefreshCallback(callback);
}

int IjkMediaPlayer::ijkmp_msg_loop(void *arg)
{
    msg_loop_(arg);
    return 0;
}

void IjkMediaPlayer::ijk_toggle_pause()
{
    if (ffplayer_->is_paused) {
        ffplayer_->ffp_resume_l();
    } else {
        ffplayer_->ffp_pause_l();
    }
}

int64_t IjkMediaPlayer::ijk_get_Total_Time()
{
    return ffplayer_->getTotalTime();
}

void IjkMediaPlayer::ijkmp_set_playback_rate(double rate)
{
    if (ffplayer_) {
        ffplayer_->setPlaybackRate(rate);
    }
}

void IjkMediaPlayer::AddProgressCallback(std::function<void(double)> callback)
{
    ffplayer_->SetProgressCallback(callback);
}

double IjkMediaPlayer::ijkmp_get_current_position_sec()
{
    return ffplayer_->get_master_clock();
}

void IjkMediaPlayer::ijkmp_set_volume(float volume)
{
    if (ffplayer_) {
        ffplayer_->setVolume(static_cast<int>(volume));
    }
}

void IjkMediaPlayer::ijkmp_set_mute(bool mute)
{
    if (ffplayer_) {
        ffplayer_->setMute(mute);
    }
}

