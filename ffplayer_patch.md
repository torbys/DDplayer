# ffplayer.cpp 需要修改的三处

## 修改 1：audio_decode_frame — 2x 时每隔一帧丢弃一帧音频

在函数开头 `frame_queue_peek_readable` 之后、data_size 计算之前，插入如下代码：

```cpp
// ===== 倍速音频跳帧 =====
// 2x 时每隔一帧丢弃一帧，使音频播放速度近似翻倍
// 原位置：audio_decode_frame(), 紧接在 frame_queue_peek_readable 成功返回之后
if (is->playback_rate_ >= 1.9) {
    is->audio_skip_counter_++;
    if (is->audio_skip_counter_ % 2 == 0) {
        // 丢弃这一帧：推进队列，输出静音
        frame_queue_next(&is->sampq);
        av_channel_layout_uninit(&dec_ch_layout);
        return -1;
    }
}
```

完整改动后的 audio_decode_frame 开头（替换原有 // 读取一帧数据 到 wanted_nb_samples 赋值 之间）：

```cpp
static int audio_decode_frame(FFPlayer *is)
{
    int data_size, resampled_data_size;
    AVChannelLayout dec_ch_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;
    int ret = 0;

    // 读取一帧数据
    if (!(af = frame_queue_peek_readable(&is->sampq)))
        return -1;

    // Serial 检查：丢弃 seek 前残留的旧帧（Bug 3 修复）
    if (af->serial != is->audioq.serial) {
        frame_queue_next(&is->sampq);
        return -1;
    }

    // ===== 倍速音频跳帧：2x 时每隔一帧丢一帧 =====
    if (is->playback_rate_ >= 1.9) {
        is->audio_skip_counter_++;
        if (is->audio_skip_counter_ % 2 == 0) {
            frame_queue_next(&is->sampq);
            return -1;
        }
    }

    // 根据frame中指定的音频参数获取缓冲区的大小
    data_size = av_samples_get_buffer_size(NULL,
                                           af->frame->ch_layout.nb_channels,
                                           af->frame->nb_samples,
                                           (enum AVSampleFormat)af->frame->format, 1);
    // ... 后续代码不变 ...
```

---

## 修改 2：compute_target_delay_l — delay 除以 playback_rate_

找到函数末尾 `return delay;` 之前，加一行：

```cpp
double FFPlayer::compute_target_delay_l(double delay, Frame *vp)
{
    double sync_threshold, diff = 0;

    if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
        diff = get_clock(&vidclk) - get_master_clock();
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    // ===== 倍速：按比例缩短帧间延迟 =====
    if (playback_rate_ > 0.01)
        delay /= playback_rate_;

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n", delay, -diff);

    return delay;
}
```

---

## 修改 3：文件末尾追加 setPlaybackRate 实现

在 `FFPlayer::SetProgressCallback` 函数之后，文件末尾追加：

```cpp
void FFPlayer::setPlaybackRate(double rate)
{
    if (rate < 0.1) rate = 0.1;   // 防止除零
    playback_rate_     = rate;
    audio_skip_counter_ = 0;      // 重置跳帧计数器

    // 速率变化时重置 frame_timer，避免旧的时间基准导致瞬间跳帧
    frame_timer = av_gettime_relative() / 1000000.0;

    qDebug() << "setPlaybackRate:" << rate;
}
```

---

## 修改 4（顺带）：ffp_seek_to_l — reset frame_timer（对应之前分析的 Bug 1）

```cpp
int FFPlayer::ffp_seek_to_l(int64_t msec)
{
    if (!ic) return -1;

    seek_pos  = msec * 1000LL;
    seek_flags = AVSEEK_FLAG_BACKWARD;
    seek_req  = 1;

    // ✅ reset frame_timer，防止 seek 后视频用旧基准狂跑
    frame_timer = av_gettime_relative() / 1000000.0;

    qDebug() << "ffp_seek_to_l: request seek to" << msec << "ms (" << seek_pos << "us)";
    return 0;
}
```

## 修改 5（顺带）：read_thread seek 处理块 — reset 两个时钟（Bug 2）

找到 seek 成功后 packet_queue_flush 调用处，改为：

```cpp
} else {
    if (audio_stream >= 0)
        packet_queue_flush(&audioq);
    if (video_stream >= 0)
        packet_queue_flush(&videoq);

    // ✅ 重置时钟，避免 audclk/vidclk 保留 seek 前的旧值
    init_clock(&audclk, &audioq.serial);
    init_clock(&vidclk, &videoq.serial);
}
```
