#ifndef IJKMEDIAPLAYER_H
#define IJKMEDIAPLAYER_H


#include <mutex>
#include <functional>
#include <thread>
#include "ffplayer.h"
#include <iostream>
#include "ffmsg_queue.h"
#include "ffmsg.h"
#include "ffplay_def.h"

// 空闲状态：未初始化或已释放资源，无任何操作
#define MP_STATE_IDLE               0

// 已初始化状态：模块已完成初始化，准备进入准备流程
#define MP_STATE_INITIALIZED        1

// 异步准备中：正在执行异步准备操作（如加载资源、建立连接）
#define MP_STATE_ASYNC_PREPARING    2

// 准备完成：资源就绪，可立即启动运行
#define MP_STATE_PREPARED           3

// 运行中：正在正常执行任务/播放/工作
#define MP_STATE_STARTED            4

// 已暂停：任务暂停，可恢复继续执行
#define MP_STATE_PAUSED             5

// 执行完成：任务正常结束，流程执行完毕
#define MP_STATE_COMPLETED          6

// 已停止：主动停止任务，可重新启动
#define MP_STATE_STOPPED            7

// 错误状态：发生异常/故障，无法继续正常工作
#define MP_STATE_ERROR              8

// 结束状态：模块生命周期结束，即将销毁
#define MP_STATE_END                9

class IjkMediaPlayer
{
public:
    IjkMediaPlayer();
    ~IjkMediaPlayer();
    int ijkmp_create(std::function<int(void *)> msg_loop);
    int ijkmp_destroy();
    // 设置要播放的url
    int ijkmp_set_data_source(const char *url);
    // 准备播放
    int ijkmp_prepare_async();
    // 触发播放
    int ijkmp_start();
    // 停止
    int ijkmp_stop();
    // 暂停
    int ijkmp_pause();
    // seek到指定位置
    int ijkmp_seek_to(long msec);
    // 获取播放状态
    int ijkmp_get_state();
    // 是不是播放中
    bool ijkmp_is_playing();
    // 当前播放位置
    long ijkmp_get_current_position();
    // 总长度
    long ijkmp_get_duration();
    // 已经播放的长度
    long ijkmp_get_playable_duration();
    // 设置循环播放
    void ijkmp_set_loop(int loop);
    // 获取是否循环播放
    int  ijkmp_get_loop();
    // 读取消息
    int ijkmp_get_msg(AVMessage *msg, int block);
    // 设置音量
    void ijkmp_set_playback_volume(float volume);
    void ijkmp_set_volume(float volume);
    void ijkmp_set_mute(bool mute);

    void AddVideoRefreshCallback(std::function<int (const Frame *)> callback);

    int ijkmp_msg_loop(void *arg);

    void ijk_toggle_pause();

    int64_t ijk_get_Total_Time();

    // ===== 倍速控制 =====
    // rate: 1.0 = 正常速度，2.0 = 2倍速
    void ijkmp_set_playback_rate(double rate);

    // 设置进度更新回调
    void AddProgressCallback(std::function<void(double)> callback);

    // 获取当前播放位置（秒）
    double ijkmp_get_current_position_sec();

private:

    // 互斥量
    std::mutex mutex_;
    // 真正的播放器
    FFPlayer *ffplayer_ = NULL;
    //函数指针，指向创建的message_loop，即消息循环函数
    //  int (*msg_loop)(void*);
    std::function<int(void *)> msg_loop_ = NULL; // ui处理消息的循环
    //消息机制线程
    std::thread *msg_thread_; // 执行msg_loop
    //   SDL_Thread _msg_thread;
    //字符串，就是一个播放urls
    char *data_source_;
    //播放器状态，例如prepared,resumed,error,completed等
    int mp_state_;  // 播放状态

};

#endif // IJKMEDIAPLAYER_H
