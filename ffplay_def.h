#ifndef FFPLAY_DEF_H
#define FFPLAY_DEF_H

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

extern "C" {
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <libavutil/bprint.h>
}

#include <SDL.h>
#include <SDL_thread.h>


#include <assert.h>

enum RET_CODE
{
    RET_ERR_UNKNOWN = -2,          // 未知错误
    RET_FAIL = -1,                 // 失败
    RET_OK = 0,                    // 正常
    RET_ERR_OPEN_FILE,             // 打开文件失败
    RET_ERR_NOT_SUPPORT,           // 不支持
    RET_ERR_OUTOFMEMORY,           // 没有内存
    RET_ERR_STACKOVERFLOW,         // 溢出
    RET_ERR_NULLREFERENCE,         // 空参考
    RET_ERR_ARGUMENTOUTOFRANGE,
    RET_ERR_PARAMISMATCH,
    RET_ERR_MISMATCH_CODE,         // 没有匹配的编解码器
    RET_ERR_EAGAIN,
    RET_ERR_EOF
};

typedef struct MyAVPacketList {
    AVPacket      pkt;             // 解封装后的数据
    struct MyAVPacketList  *next;  // 下一个节点
    int           serial;          // 播放序列
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList  *first_pkt, *last_pkt;  // 队首，队尾指针
    int             nb_packets;             // 包数量，也就是队列元素数量
    int             size;                   // 队列所有元素的数据大小总和
    int64_t         duration;               // 队列所有元素的数据播放持续时间
    int             abort_request;          // 用户退出请求标志
    int             serial;                 // 播放序列号，和MyAVPacketList的serial作用相同，但改变的时序稍微有点不同
    SDL_mutex       *mutex;                 // 用于维持PacketQueue的多线程安全(SDL_mutex可以按pthread_mutex_t理解）
    SDL_cond        *cond;                  // 用于读、写线程相互通知(SDL_cond可以按pthread_cond_t理解)
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE       3       // 图像帧缓存数量
#define VIDEO_PICTURE_QUEUE_SIZE_MIN   (3)
#define VIDEO_PICTURE_QUEUE_SIZE_MAX   (16)
#define VIDEO_PICTURE_QUEUE_SIZE_DEFAULT (VIDEO_PICTURE_QUEUE_SIZE_MIN)
#define SUBPICTURE_QUEUE_SIZE          16      // 字幕帧缓存数量
#define SAMPLE_QUEUE_SIZE              9       // 采样帧缓存数量
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

typedef struct AudioParams {
    int             freq;               // 采样率
    //int             channels;           // 通道数
    AVChannelLayout ch_layout;     // 通道布局，比如2.1声道，5.1声道等
    enum AVSampleFormat fmt;            // 音频采样格式，比如AV_SAMPLE_FMT_S16表示为有符号16bit深度，交错排列模式。
    int             frame_size;         // 一个采样单元占用的字节数（比如2通道时，则左右通道各采样一次合成一个采样单元）
    int             bytes_per_sec;      // 一秒时间的字节数，比如采样率48Khz，2 channel，16bit，则一秒48000*2*16/8=192000
} AudioParams;

/* Common struct for handling all types of decoded data and allocated render buffers. */
// 用于缓存解码后的数据
typedef struct Frame {
    AVFrame     *frame;         // 指向数据帧
    double      pts;            // 时间戳，单位为秒
    double      duration;       // 该帧持续时间，单位为秒
    int         width;          // 图像宽度
    int         height;         // 图像高度
    int         format;         // 对于图像为(enum AVPixelFormat)
    int         serial;         // 【新增】该帧所属的播放序列
    int64_t     pos;
} Frame;

/* 这是一个循环队列，windex是指其中的首元素，rindex是指其中的尾部元素. */
typedef struct FrameQueue {
    Frame       queue[FRAME_QUEUE_SIZE];  // FRAME_QUEUE_SIZE 最大size，数字太大时会占用大量的内存，需要注意该值的设置
    int         rindex;                  // 读索引。待播放时读取此帧进行播放，播放后此帧成为上一帧
    int         rindex_shown;            // 读索引是否已显示，用于keep_last机制
    int         windex;                  // 写索引
    int         size;                    // 当前总帧数
    int         max_size;                // 可存储最大帧数
    int         keep_last;               // 是否保留上一帧，视频/音频为1，字幕为0
    SDL_mutex   *mutex;                  // 互斥量
    SDL_cond    *cond;                   // 条件变量
    PacketQueue *pktq;                   // 数据包缓冲队列
} FrameQueue;

typedef struct Clock {
    double pts;           // 时钟基础，当前帧(待播放)显示时间戳，播放后，当前帧变成上一帧
    // 当前pts与当前系统时钟的差值，audio、video对于该值是独立的
    double pts_drift;     // clock base minus time at which we updated the clock
    // 当前时钟(如视频时钟)最后一次更新时间，也可称当前时钟时间
    double last_updated;  // 最后一次更新的系统时钟

    int serial;         //序列

    int *queue_serial;   //队序列

    bool paused;
} Clock;

enum {
    AV_SYNC_UNKNOW_MASTER = -1,
    AV_SYNC_AUDIO_MASTER,               // 以音频为基准
    AV_SYNC_VIDEO_MASTER,               // 以视频为基准
    // AV_SYNC_EXTERNAL_CLOCK,            // 以外部时钟为基准，synchronize to an external clo
};

// 队列相关
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
int packet_queue_init(PacketQueue *q);
void packet_queue_flush(PacketQueue *q);
void packet_queue_destroy(PacketQueue *q);
void packet_queue_abort(PacketQueue *q);
void packet_queue_start(PacketQueue *q);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
void packet_queue_inc_serial(PacketQueue *q);
//int packet_queue_nb_packets(PacketQueue *q);

/* 初始化FrameQueue，视频和音频keep_last设置为1，字幕设置为0 */
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size);
void frame_queue_destory(FrameQueue *f);
void frame_queue_signal(FrameQueue *f);


/* 获取队列当前Frame，在调用该函数前先调用frame_queue_nb_remaining确保有frame可读 */
Frame *frame_queue_peek(FrameQueue *f);

/* 获取当前Frame的下一Frame，此时要确保queue里面至少有2个Frame */
// 不管你什么时候调用，返回回来肯定不是NULL
Frame *frame_queue_peek_next(FrameQueue *f);

/* 获取last Frame：*/
Frame *frame_queue_peek_last(FrameQueue *f);

// 获取可写指针
Frame *frame_queue_peek_writable(FrameQueue *f);
// 获取可读
Frame *frame_queue_peek_readable(FrameQueue *f);
// 更新写指针
void frame_queue_push(FrameQueue *f);
/* 释放当前frame，并更新读索引rindex */
void frame_queue_next(FrameQueue *f);
int frame_queue_nb_remaining(FrameQueue *f);
int64_t frame_queue_last_pos(FrameQueue *f);

//冲刷空包
void frame_queue_flush(FrameQueue *f);

// 时钟相关
double get_clock(Clock *c);
void set_clock_at(Clock *c, double pts, double time,int serial);
void set_clock(Clock *c, double pts,int serial);
void init_clock(Clock *c,int *queue_serial);


#endif // FFPLAY_DEF_H
