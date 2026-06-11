#ifndef FFPLAYER_H
#define FFPLAYER_H

#include <mutex>
#include <functional>
#include <thread>
#include "ffmsg_queue.h"
#include <iostream>
#include "ffplay_def.h"
#include "ffmsg.h"
#include "SoundTouch.h"


#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0



class Decoder
{
public:
    AVPacket pkt_;
    PacketQueue *queue_;        // 数据包队列
    AVCodecContext  *avctx_;    // 解码器上下文
    int     pkt_serial_;        // 包序列
    int     finished_;          // =0，解码器处于工作状态；=非0，解码器处于空闲状态
    std::thread *decoder_thread_ = NULL;
    int decoder_reorder_pts = -1;   //选择PTS

    Decoder();
    ~Decoder();
    void decoder_init(AVCodecContext *avctx, PacketQueue *queue);
    // 创建和启动线程
    int decoder_start(enum AVMediaType codec_type, const char *thread_name, void* arg);
    // 停止线程
    void decoder_abort(FrameQueue *fq);
    void decoder_destroy();
    int decoder_decode_frame(AVFrame *frame);
    int get_video_frame_l(AVFrame *frame,void *arg);
    int queue_picture(FrameQueue *fq, AVFrame *src_frame, double pts,
                      double duration, int64_t pos, int serial);

    int audio_thread(void* arg);
    int video_thread(void* arg);
};

class FFPlayer
{
public:
    FFPlayer();

    // ============================================================
    // 一、生命周期管理（创建/销毁/打开/关闭）
    // ============================================================
    int ffp_create();
    void ffp_destroy();
    int ffp_prepare_async_l(char *file_name);

    int stream_open(const char *file_name);
    void stream_close();

    // 打开指定stream对应解码器、创建解码线程、以及初始化对应的输出
    int stream_component_open(int stream_index);
    // 关闭指定stream的解码线程，释放解码器资源
    void stream_component_close(int stream_index);

    // ============================================================
    // 二、播放控制（播放/暂停/停止/Seek）
    // ============================================================
    int ffp_start_l();
    int ffp_stop_l();
    int ffp_seek_to_l(int64_t msec);

    int ffp_pause_l();      // 暂停
    int ffp_resume_l();     // 恢复
    bool is_paused = false;

    int paused;             // 是否暂停（内部状态标志）

    // ============================================================
    // 三、音视频输出管理（SDL音频/OpenGL视频）
    // ============================================================
    // 音频输出
    int audio_open(AVChannelLayout *wanted_ch_layout, int wanted_sample_rate, AudioParams *audio_hw_params);
    void audio_close();

    // 视频刷新线程
    int video_refresh_thread();
    void video_refresh(double *remaining_time);
    std::thread *video_refresh_thread_ = NULL;

    // 视频帧输出回调（供 UI 层注册，如 VideoGLWidget）
    std::function<int(const Frame *)> video_refresh_callback_ = NULL;
    void AddVideoRefreshCallback(std::function<int(const Frame *)> callback);

    // ============================================================
    // 四、音视频同步（时钟/延迟计算/丢帧）
    // ============================================================
    int get_master_sync_type();
    double get_master_clock();

    int av_sync_type = AV_SYNC_AUDIO_MASTER;  // 音视频同步类型, 默认audio master
    Clock audclk;                             // 音频时钟
    Clock vidclk;                             // 视频时钟

    double audio_clock = 0;                   // 当前音频帧的PTS+当前帧Duration

    double frame_timer = 0;
    double frame_timer_pause_offset = 0;      // 暂停时frame_timer的偏移量
    double max_frame_duration;
    int frame_drops_late = 0;                 // 丢帧统计
    int frame_drops_early;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int framedrop = 1;
    int step;

    double compute_target_delay_l(double delay, Frame *vp);
    double vp_duration(Frame *vp, Frame *nextvp);
    void step_to_next_frame();

    double audio_pts_offset = NAN;            // 音频流 PTS 偏移（FLV 兼容）
    double video_pts_offset = NAN;            // 视频流 PTS 偏移（FLV 兼容）

    // ============================================================
    // 五、变速播放（SoundTouch 变速不变调）
    // ============================================================
    void setPlaybackRate(double rate);
    double playback_rate_ = 1.0;

    soundtouch::SoundTouch soundTouch_;
    bool soundtouch_initialized_ = false;

    // 变速后的输出缓冲（替代原 audio_buf1 用于重采样后的数据）
    uint8_t  *st_buf_  = nullptr;             // SoundTouch 输出缓冲
    unsigned  st_buf_size_ = 0;
    std::vector<float> st_float_in_buf_;      // 预分配，避免回调里频繁 malloc
    std::vector<float> st_float_out_buf_;

    // ============================================================
    // 六、音量控制（静音/音量大小）
    // ============================================================
    void setVolume(int volume);
    int getVolume() const;
    void setMute(bool mute);
    bool isMuted() const;

    int audio_volume = 50;                    // 音量 0~100，默认50
    bool audio_muted = false;                 // 是否静音
    int volume_before_mute = 100;             // 静音前的音量值（用于恢复）

    // ============================================================
    // 七、UI 回调接口（进度/总时长）
    // ============================================================
    int64_t getTotalTime();
    int64_t Total_time = -1;

    std::function<void(double)> progress_callback_ = nullptr;
    void SetProgressCallback(std::function<void(double)> callback);

    // ============================================================
    // 八、核心数据队列与解码器
    // ============================================================
    // 帧队列
    FrameQueue pictq;                         // 视频Frame队列
    FrameQueue sampq;                         // 采样Frame队列

    // 包队列
    PacketQueue audioq;                       // 音频packet队列
    PacketQueue videoq;                       // 视频packet队列

    // 解码器
    Decoder auddec;
    Decoder viddec;

    // ============================================================
    // 九、FFmpeg 核心对象（流/格式上下文）
    // ============================================================
    AVFormatContext *ic = NULL;               // 格式上下文（解复用）

    AVStream *audio_st = NULL;                // 音频流
    AVStream *video_st = NULL;                // 视频流

    int audio_stream = -1;                    // 音频流索引
    int video_stream = -1;                    // 视频流索引

    // ============================================================
    // 十、音频重采样与缓冲区（SDL 音频回调用）
    // ============================================================
    struct AudioParams audio_src;             // 音频frame的参数
    struct AudioParams audio_tgt;             // SDL支持的音频参数，重采样转换：audio_src->audio_tgt
    struct SwrContext *swr_ctx = NULL;        // 音频重采样context
    int audio_hw_buf_size = 0;                // SDL音频缓冲区的大小(字节为单位)

    // 指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区
    uint8_t *audio_buf = NULL;                // 指向需要重采样的数据
    uint8_t *audio_buf1 = NULL;               // 指向重采样后的数据
    unsigned int audio_buf_size = 0;          // 待播放的一帧音频数据(audio_buf指向)的大小
    unsigned int audio_buf1_size = 0;         // 申请到的音频缓冲区audio_buf1的实际尺寸
    int audio_buf_index = 0;                  // 更新拷贝位置 当前音频帧中已拷入SDL音频缓冲区

    int64_t audio_callback_time;
    int audio_write_buf_size;

    // ============================================================
    // 十一、线程与消息队列
    // ============================================================
    int read_thread();
    std::thread *read_thread_;

    MessageQueue msg_queue_;
    char* input_filename_;

    int abort_request = 0;                    // 退出请求标志
    int eof = 0;                              // 文件读取结束标志

    // ============================================================
    // 十二、Seek 相关状态
    // ============================================================
    int seek_req = 0;                         // seek 请求标志
    int64_t seek_pos = 0;                     // seek 目标位置
    int seek_flags = 0;                       // seek 标志（AVSEEK_FLAG_BACKWARD 等）
    int seek_serial = 0;                      // seek 时的 serial
    int64_t seek_rel = 0;
};

inline static void ffp_notify_msg1(FFPlayer *ffp, int what) {
    msg_queue_put_simple3(&ffp->msg_queue_, what, 0, 0);
}

inline static void ffp_notify_msg2(FFPlayer *ffp, int what, int arg1) {
    msg_queue_put_simple3(&ffp->msg_queue_, what, arg1, 0);
}

inline static void ffp_notify_msg3(FFPlayer *ffp, int what, int arg1, int arg2) {
    msg_queue_put_simple3(&ffp->msg_queue_, what, arg1, arg2);
}

inline static void ffp_notify_msg4(FFPlayer *ffp, int what, int arg1, int arg2, void *obj, int obj_len) {
    msg_queue_put_simple4(&ffp->msg_queue_, what, arg1, arg2, obj, obj_len);
}

inline static void ffp_remove_msg(FFPlayer *ffp, int what) {
    msg_queue_remove(&ffp->msg_queue_, what);
}

#endif // FFPLAYER_H
