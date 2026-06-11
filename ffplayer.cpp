#include "ffplayer.h"

#include <qDebug>
#include <algorithm>

#define SDL_AUDIO_MIN_BUFFER_SIZE 512

FFPlayer::FFPlayer() {}

void print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}

int FFPlayer::ffp_create()
{
    std::cout << "ffp_create\n";
    msg_queue_init(&msg_queue_);
    return 0;
}

void FFPlayer::ffp_destroy()
{
    stream_close();
    msg_queue_destroy(&msg_queue_);
}

int FFPlayer::ffp_prepare_async_l(char *file_name)
{
    // //保存文件名
    input_filename_ = strdup(file_name);

    qDebug()<<"input_filename_ : "<<input_filename_;

    int reval = stream_open(file_name);

    return reval;
}

int FFPlayer::ffp_start_l()
{
    // 触发播放
    std::cout << __FUNCTION__;

    // // 启动音频播放
    // if (audio_stream >= 0) {
    //     SDL_PauseAudio(0);  // 0 = 播放, 1 = 暂停
    //     std::cout << "SDL_PauseAudio(0) called" << std::endl;
    // }

    return 0;
}

int FFPlayer::ffp_stop_l()
{
    abort_request = 1;    // 请求退出
    msg_queue_abort(&msg_queue_);    // 禁止再插入消息
    return 0;
}

int FFPlayer::ffp_seek_to_l(int64_t msec)
{
    if (!ic) {
        return -1;
    }

    // 转换为微秒（AV_TIME_BASE）
    seek_pos = msec * 1000LL;
    seek_flags = AVSEEK_FLAG_BACKWARD;  // 优先向后查找关键帧
    seek_req = 1;

    frame_timer = av_gettime_relative() / 1000000.0;

    qDebug() << "ffp_seek_to_l: request seek to" << msec << "ms (" << seek_pos << "us)";
    return 0;
}

int FFPlayer::stream_open(const char *file_name)
{

#ifdef _WIN32
    putenv("SDL_AUDIODRIVER=directsound");
    printf("Forced SDL audio driver to: directsound\n");
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        return -1;
    }
    // 初始化Frame帧队列
    if (frame_queue_init(&pictq, &videoq, VIDEO_PICTURE_QUEUE_SIZE_DEFAULT) < 0)
        goto fail;
    if (frame_queue_init(&sampq, &audioq, SAMPLE_QUEUE_SIZE) < 0)
        goto fail;

    // 初始化Packet包队列
    if (packet_queue_init(&videoq) < 0 ||
        packet_queue_init(&audioq) < 0)
        goto fail;

    // 初始化时钟
    init_clock(&audclk,&audioq.serial);
    init_clock(&vidclk,&videoq.serial);

    // 初始化音量等

    // 创建解复用器读数据线程read_thread
    read_thread_ = new std::thread(&FFPlayer::read_thread, this);

    // 创建视频刷新线程
    video_refresh_thread_ = new std::thread(&FFPlayer::video_refresh_thread,this);

    return 0;
fail:
    stream_close();
    return -1;
}

void FFPlayer::stream_close()
{
    abort_request = 1;  // 请求退出

    if(read_thread_ && read_thread_->joinable()) {
        read_thread_->join();
        delete read_thread_;
        read_thread_ = NULL;
    }

    if(video_refresh_thread_ && video_refresh_thread_->joinable()) {
        video_refresh_thread_->join();
        delete video_refresh_thread_;
        video_refresh_thread_ = NULL;
    }

    /* close each stream */
    if (audio_stream >= 0)
        stream_component_close(audio_stream); // 解码器线程请求abort的时候有调用 packet_queue_abort
    if (video_stream >= 0)
        stream_component_close(video_stream);

    // 关闭解复用器 avformat_close_input(&is->ic);
    if(ic) {
        avformat_close_input(&ic);
        ic = NULL;
    }

    // 释放packet队列
    packet_queue_destroy(&videoq);
    packet_queue_destroy(&audioq);

    // 释放frame队列
    frame_queue_destory(&pictq);
    frame_queue_destory(&sampq);

    SDL_Quit();

    // 重置关键成员变量，确保下次运行干净状态
    audio_stream = -1;
    video_stream = -1;
    audio_st = NULL;
    video_st = NULL;
    frame_timer = 0;
    eof = 0;
    audio_clock = NAN;
    seek_req = 0;
    paused = 0;
    is_paused = false;
    frame_drops_late = 0;
    frame_drops_early = 0;
    Total_time = -1;
    abort_request = 0;

    if (st_buf_) {
        av_freep(&st_buf_);
        st_buf_size_ = 0;
    }
    soundtouch_initialized_ = false;
    soundTouch_.clear();

    if(input_filename_) {
        free(input_filename_);
        input_filename_ = NULL;
    }
}

int FFPlayer::stream_component_open(int stream_index)
{
    AVCodecContext *avctx;
    const AVCodec *codec;
    int sample_rate;
    int nb_channels;
    int64_t channel_layout;
    int ret = 0;

    qDebug()<<"stream_component_open into";

    // 判断stream_index是否合法
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    /* 为解码器分配一个编解码器上下文结构体 */
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    /* 将码流中的编解码器信息拷贝到新分配的编解码器上下文结构体 */
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;

    // 设置pkt_timebase
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;



    /* 根据codec_id查找解码器 */
    codec = avcodec_find_decoder(avctx->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_WARNING,
               "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        goto fail;
    }

    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) {
        goto fail;
    }

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        // 从avctx(即AVCodecContext)中获取音频格式参数
        sample_rate = avctx->sample_rate;    // 采样率
        // 7.x: 使用 ch_layout 替代 channels 和 channel_layout
        nb_channels = avctx->ch_layout.nb_channels;  // 通道数

        /* prepare audio output 准备音频输出 */
        //调用audio_open打开sdl音频输出，实际打开的设备参数保存在audio_tgt，返回值表示输出设备的缓冲区大小
        if ((ret = audio_open(&avctx->ch_layout, sample_rate, &audio_tgt)) < 0)
            goto fail;

        audio_hw_buf_size = ret;
        audio_src = audio_tgt;    //暂且将数据源参数等同于目标输出参数

        //初始化audio_buf相关参数
        audio_buf_size   = 0;
        audio_buf_index = 0;

        audio_stream = stream_index;    // 获取audio的stream索引
        audio_st = ic->streams[stream_index];    // 获取audio的stream指针

        // 初始化ffplay封装的音频解码器, 并将解码器上下文avctx和Decoder绑定
        // decoder_init
        auddec.decoder_init(avctx,&audioq);

        // 启动音频解码线程
        // decoder_start
        auddec.decoder_start(AVMEDIA_TYPE_AUDIO,"audio thread",this);

        // 允许音频输出
        qDebug()<<"音频输出设置成功";
        SDL_PauseAudio(0);
        break;

    case AVMEDIA_TYPE_VIDEO:
        video_stream = stream_index;    // 获取video的stream索引
        video_st = ic->streams[stream_index];

        //Total_time = ic->streams[stream_index]->duration;
        // 初始化视频解码器
        viddec.decoder_init(avctx, &videoq); // is->continue_read_thread
        // 启动视频解码线程
        if ((ret = viddec.decoder_start(AVMEDIA_TYPE_VIDEO, "video_decoder", this)) < 0)
            goto out;

        break;

    default:
        break;
    }

    goto out;
fail:
    avcodec_free_context(&avctx);
out:
    return ret;
}

void FFPlayer::stream_component_close(int stream_index)
{
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        std::cout << __FUNCTION__ << " AVMEDIA_TYPE_AUDIO\n";
        // 请求终止解码器线程
        auddec.decoder_abort(&sampq);
        // 关闭音频设备
        audio_close();
        // 销毁解码器
        auddec.decoder_destroy();
        // 释放重采样器

        swr_free(&swr_ctx);

        // 释放audio buf

        if(audio_buf1 != NULL){
            av_freep(&audio_buf1);
        }
        audio_buf1_size = 0;
        audio_buf = NULL;

        // 关闭音频输出
        SDL_PauseAudio(1);
        break;

    case AVMEDIA_TYPE_VIDEO:
        std::cout << __FUNCTION__ << " AVMEDIA_TYPE_VIDEO\n";

        // 关闭音频设备
        viddec.decoder_abort(&pictq);

        // 销毁解码器
        // decoder_abort(&is->viddec, &is->pictq);
        viddec.decoder_destroy();
        break;

    default:
        break;
    }

    // ic->streams[stream_index]->discard = AVDISCARD_ALL; // 这个又有什么用?

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        audio_st = NULL;
        audio_stream = -1;
        break;

    case AVMEDIA_TYPE_VIDEO:
        video_st = NULL;
        video_stream = -1;
        break;

    default:
        break;
    }
}

static int audio_decode_frame(FFPlayer *is)
{
    int data_size, resampled_data_size;
    AVChannelLayout dec_ch_layout;  // FFmpeg 7.x: 改为 AVChannelLayout 结构体
    av_unused double audio_clock;
    int wanted_nb_samples;
    Frame *af;
    int ret = 0;

    // 读取一帧数据
    if (!(af = frame_queue_peek_readable(&is->sampq)))
        return -1;

    // ✅ 新增：丢弃 seek 之前残留的旧帧
    if (af->serial != is->audioq.serial) {
        frame_queue_next(&is->sampq);
        return -1;  // 让 sdl_audio_callback 输出静音，等待新帧
    }

    double original_pts = af->pts;
    int original_nb_samples = af->frame->nb_samples;
    int original_sample_rate  = af->frame->sample_rate;  // ← 新增，在出队前保存

    int st_received = 0;

    // 根据frame中指定的音频参数获取缓冲区的大小
    data_size = av_samples_get_buffer_size(NULL,
                                           af->frame->ch_layout.nb_channels,  // 7.x: 改为 ch_layout.nb_channels
                                           af->frame->nb_samples,
                                           (enum AVSampleFormat)af->frame->format, 1);

    // 初始化 dec_ch_layout
    // 如果 frame 有有效的 ch_layout，直接复制；否则使用默认布局
    if (af->frame->ch_layout.nb_channels > 0 &&
        af->frame->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_copy(&dec_ch_layout, &af->frame->ch_layout);
    } else {
        av_channel_layout_default(&dec_ch_layout, af->frame->ch_layout.nb_channels);
    }

    // 获取样本数校正值
    wanted_nb_samples = af->frame->nb_samples;

    // 检查是否需要重采样：比较格式、声道布局、采样率
    if (af->frame->format         != is->audio_src.fmt          ||
        av_channel_layout_compare(&dec_ch_layout, &is->audio_src.ch_layout) ||  // 7.x: 用函数比较
        af->frame->sample_rate    != is->audio_src.freq
        ) {
        swr_free(&is->swr_ctx);

        // 7.x: 使用 swr_alloc_set_opts2，参数改为 AVChannelLayout*
        ret = swr_alloc_set_opts2(&is->swr_ctx,
                                  &is->audio_tgt.ch_layout,   // 目标输出
                                  is->audio_tgt.fmt,
                                  is->audio_tgt.freq,
                                  &dec_ch_layout,             // 数据源
                                  (enum AVSampleFormat)af->frame->format,
                                  af->frame->sample_rate,
                                  0, NULL);

        if (ret < 0 || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                   af->frame->sample_rate,
                   av_get_sample_fmt_name((enum AVSampleFormat)af->frame->format),
                   af->frame->ch_layout.nb_channels,  // 7.x
                   is->audio_tgt.freq,
                   av_get_sample_fmt_name(is->audio_tgt.fmt),
                   is->audio_tgt.ch_layout.nb_channels);  // 7.x
            swr_free(&is->swr_ctx);
            ret = -1;
            goto fail;
        }

        // 保存源音频参数
        av_channel_layout_copy(&is->audio_src.ch_layout, &dec_ch_layout);  // 7.x
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (enum AVSampleFormat)af->frame->format;

        // ===== 重采样参数改变时重置 SoundTouch =====
        is->soundTouch_.clear();  // 先清除旧数据，避免残留
        is->soundTouch_.setSampleRate(is->audio_tgt.freq);
        is->soundTouch_.setChannels(is->audio_tgt.ch_layout.nb_channels);
        is->soundTouch_.setTempo(is->playback_rate_);

        // ===== 关键：优化音质的参数 =====
        // SEQUENCE_MS 用默认82ms，片段够长拼接质量更好，颗粒感明显减少
        is->soundTouch_.setSetting(SETTING_SEQUENCE_MS, 82);
        // seekWindowMs 加大搜索窗口，让拼接点选择更准确
        is->soundTouch_.setSetting(SETTING_SEEKWINDOW_MS, 28);
        // overlapMs 加大重叠长度，过渡更平滑
        is->soundTouch_.setSetting(SETTING_OVERLAP_MS, 12);

        is->soundtouch_initialized_ = true;
    }

    // 确保 SoundTouch 初始化过一次
    if (!is->soundtouch_initialized_) {
        is->soundTouch_.setSampleRate(is->audio_tgt.freq);
        is->soundTouch_.setChannels(is->audio_tgt.ch_layout.nb_channels);
        is->soundTouch_.setTempo(is->playback_rate_);
        // SEQUENCE_MS 用默认82ms，片段够长拼接质量更好，颗粒感明显减少
        is->soundTouch_.setSetting(SETTING_SEQUENCE_MS, 82);
        // seekWindowMs 加大搜索窗口，让拼接点选择更准确
        is->soundTouch_.setSetting(SETTING_SEEKWINDOW_MS, 28);
        // overlapMs 加大重叠长度，过渡更平滑
        is->soundTouch_.setSetting(SETTING_OVERLAP_MS, 12);
        is->soundtouch_initialized_ = true;
    }

    if (is->swr_ctx) {
        // 重采样输入参数
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;

        // 重采样输出参数
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size  = av_samples_get_buffer_size(NULL,
                                                  is->audio_tgt.ch_layout.nb_channels,  // 7.x
                                                  out_count,
                                                  is->audio_tgt.fmt, 0);

        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            ret = -1;
            goto fail;
        }

        // 分配缓冲区
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        // 音频重采样
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            ret = -1;
            goto fail;
        }

        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }

        // 重采样后的数据
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.ch_layout.nb_channels *  // 7.x
                              av_get_bytes_per_sample(is->audio_tgt.fmt);
    } else {
        // 无需重采样
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    frame_queue_next(&is->sampq);

    // // 更新音频时钟
    // if (!isnan(af->pts))
    //     is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    // else
    //     is->audio_clock = NAN;



    // ===== SoundTouch 变速处理 =====
    // 只在速率不是 1.0 时走 SoundTouch，正常速率直接输出节省 CPU
    if (fabs(is->playback_rate_ - 1.0) > 0.01) {
        int channels         = is->audio_tgt.ch_layout.nb_channels;
        int bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt.fmt);
        int num_samples_in   = resampled_data_size / (bytes_per_sample * channels);

        // 预分配缓冲，只在尺寸不够时才 resize（均摊开销）
        int float_buf_count  = num_samples_in * channels;
        int max_out_count    = (num_samples_in + 4096) * channels;
        if ((int)is->st_float_in_buf_.size()  < float_buf_count) is->st_float_in_buf_.resize(float_buf_count);
        if ((int)is->st_float_out_buf_.size() < max_out_count)   is->st_float_out_buf_.resize(max_out_count);

        // int16 → float
        const int16_t *src = reinterpret_cast<const int16_t*>(is->audio_buf);
        for (int i = 0; i < float_buf_count; i++)
            is->st_float_in_buf_[i] = src[i] / 32768.0f;

        is->soundTouch_.putSamples(is->st_float_in_buf_.data(), num_samples_in);

        int max_out_samples = num_samples_in + 4096;
        st_received = is->soundTouch_.receiveSamples(
            is->st_float_out_buf_.data(), max_out_samples);

        if (st_received == 0) {
            ret = -1;
            goto fail;
        }

        int st_buf_bytes = st_received * channels * bytes_per_sample;
        av_fast_malloc(&is->st_buf_, &is->st_buf_size_, st_buf_bytes);
        if (!is->st_buf_) { ret = AVERROR(ENOMEM); goto fail; }

        int16_t *dst      = reinterpret_cast<int16_t*>(is->st_buf_);
        int      total_out = st_received * channels;
        for (int i = 0; i < total_out; i++) {
            float v = is->st_float_out_buf_[i] * 32768.0f;
            v = v >  32767.0f ?  32767.0f : (v < -32768.0f ? -32768.0f : v);
            dst[i] = static_cast<int16_t>(v);
        }

        is->audio_buf       = is->st_buf_;
        resampled_data_size = st_buf_bytes;

    }

    // SoundTouch 处理完后再更新 audio_clock
    // 1x 时用 original_nb_samples，变速时用 received（实际输出样本数）
    if (!isnan(original_pts)) {
        if (fabs(is->playback_rate_ - 1.0) > 0.01) {
            // received 在上面的 SoundTouch 块里赋值，这里需要能访问到它
            // 把 received 提升到函数作用域（见下面说明）
            is->audio_clock = original_pts + (double)st_received / is->audio_tgt.freq;
        } else {
            is->audio_clock = original_pts + (double)original_nb_samples / original_sample_rate;
        }
    } else {
        is->audio_clock = NAN;
    }

    ret = resampled_data_size;

fail:
    // 清理局部分配的 ch_layout（如果使用了默认分配）
    av_channel_layout_uninit(&dec_ch_layout);  // 7.x: 需要清理

    return ret;
}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    FFPlayer *is = (FFPlayer *)opaque;
    int audio_size, len1;

    is->audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(is);
            if (audio_size < 0) {
                // 错误时输出静音，并等待一段时间避免CPU占用过高
                is->audio_buf = NULL;
                is->audio_buf_size = 512; // 设置一个非零值，让循环继续
                // 或者直接用 SDL_Delay(1) 但回调里不能用 SDL_Delay
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }

        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;

        if (is->audio_buf) {
            if (is->audio_muted) {
                memset(stream, 0, len1);
            } else {
                memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
                //通过采样率的音量调整
                //volume_factor = 75/100 = 0.75对每个 int16 采样点 × 0.75 → 音量变为 75%
                if (!is->audio_muted && is->audio_volume < 100) {
                    float volume_factor = static_cast<float>(is->audio_volume) / 100.0f;
                    int16_t *samples = reinterpret_cast<int16_t *>(stream);
                    int sample_count = len1 / sizeof(int16_t);
                    for (int i = 0; i < sample_count; i++) {
                        int sample = static_cast<int>(samples[i] * volume_factor);
                        samples[i] = static_cast<int16_t>(std::max(-32768, std::min(32767, sample)));
                    }
                }
            }
        }else{
            memset(stream, 0, len1);
        }

        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }

    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;

    double hw_delay;

    if (!isnan(is->audio_clock)) {
        // 2x 时实际输出字节速率 = bytes_per_sec * playback_rate_
        // 硬件缓冲对应的时间延迟要除以实际速率才正确
        hw_delay = (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size)
                          / (is->audio_tgt.bytes_per_sec);
        set_clock_at(&is->audclk,
                     is->audio_clock - hw_delay,
                     is->audio_callback_time / 1000000.0,
                     is->auddec.pkt_serial_);
    }

    // if (!isnan(is->audio_clock)) {
    //     // 2x 时实际输出字节速率 = bytes_per_sec * playback_rate_
    //     // 硬件缓冲对应的时间延迟要除以实际速率才正确
    //     hw_delay = (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size)
    //                / (is->audio_tgt.bytes_per_sec);
    //     set_clock_at(&is->audclk,
    //                  is->audio_clock - hw_delay,
    //                  is->auddec.pkt_serial_,
    //                  is->audio_callback_time / 1000000.0);
    // }

}

int FFPlayer::audio_open(AVChannelLayout *wanted_ch_layout, int wanted_sample_rate, AudioParams *audio_hw_params)
{
    SDL_AudioSpec wanted_spec;
    int wanted_nb_channels = wanted_ch_layout->nb_channels;  // 7.x: 从 AVChannelLayout 获取通道数

    // 音频参数设置 SDL_AudioSpec
    wanted_spec.freq = wanted_sample_rate;          // 采样频率
    wanted_spec.format = AUDIO_S16SYS;            // 采样点格式
    wanted_spec.channels = wanted_nb_channels;      // 通道数
    wanted_spec.silence = 0;
    wanted_spec.samples = 2048;                     // 每次读取的采样数量
    wanted_spec.callback = sdl_audio_callback;      // 回调函数
    wanted_spec.userdata = this;

    // 打开音频设备
    if (SDL_OpenAudio(&wanted_spec, NULL) != 0)
    {
        printf("Failed to open audio device, %s\n", SDL_GetError());
        return -1;
    }

    // audio_hw_params 保存重采样目标格式
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = wanted_spec.freq;

    // 7.x: 复制 AVChannelLayout 到 audio_hw_params
    av_channel_layout_copy(&audio_hw_params->ch_layout, wanted_ch_layout);
    // 注意：audio_hw_params 中不再有单独的 channels 字段，通过 ch_layout.nb_channels 访问

    /* 计算一个采样点占用的字节数 */
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL,
                                                             audio_hw_params->ch_layout.nb_channels,  // 7.x
                                                             1,
                                                             audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL,
                                                                audio_hw_params->ch_layout.nb_channels,  // 7.x
                                                                audio_hw_params->freq,
                                                                audio_hw_params->fmt, 1);

    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }

    return wanted_spec.size;   /* SDL 内部缓存的数据字节 */
}



void FFPlayer::audio_close()
{
    SDL_CloseAudio();
}

int FFPlayer::read_thread()
{
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];    // AVMEDIA_TYPE_VIDEO/ AVMEDIA_TYPE_AUDIO 等, 用来保存stream index
    AVPacket pkt1;
    AVPacket *pkt = &pkt1;

    // 初始化为-1,如果一直为-1说明没相应steam
    memset(st_index, -1, sizeof(st_index));
    video_stream = -1;
    audio_stream = -1;
    eof = 0;

    // 1. 创建上下文结构体，这个结构体是最上层的结构体，表示输入上下文
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    /* 3.打开文件，主要是探测协议类型，如果是网络文件则创建网络链接等 */
    err = avformat_open_input(&ic, input_filename_, NULL, NULL);
    if (err < 0) {
        print_error(input_filename_, err);
        ret = -1;
        goto fail;
    }

    ffp_notify_msg1(this, FFP_MSG_OPEN_INPUT);
    std::cout << "read_thread FFP_MSG_OPEN_INPUT " << this << std::endl;

    /*
     * 4. 探测媒体类型，可得到当前文件的封装格式，音视频编码参数等信息
     * 调用该函数后得多的参数信息会比只调用avformat_open_input更为详细，
     * 其本质上是去做了decode packet获取信息的工作
     * codecpar, filled by libavformat on stream creation or
     * in avformat_find_stream_info()
     */
    err = avformat_find_stream_info(ic, NULL);
    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING,
               "%s: could not find codec parameters\n", input_filename_);
        ret = -1;
        goto fail;
    }

    // 替换原来的 for 循环时长获取逻辑
    // 优先用容器级别的 duration（mp4/mkv 等格式都有）
    if (ic->duration != AV_NOPTS_VALUE && ic->duration > 0) {
        // ic->duration 单位是 AV_TIME_BASE (microseconds)
        Total_time = (double)ic->duration / AV_TIME_BASE;
        qDebug() << "Total time from ic->duration:" << Total_time;
    } else {
        // ic->duration 无效时（FLV 等流式格式），遍历 stream 取最大值
        for (int i = 0; i < ic->nb_streams; i++) {
            AVStream *st = ic->streams[i];
            if (st->duration != AV_NOPTS_VALUE && st->duration > 0) {
                double d = st->duration * av_q2d(st->time_base);
                if (d > Total_time) {
                    Total_time = d;
                    qDebug() << "Total time from stream" << i << ":" << Total_time;
                }
            }
        }
    }

    // FLV 兜底：如果以上都没有，avformat_find_stream_info 之后
    // 再尝试一次 ic->duration（部分 FLV 在 find_stream_info 后才被填充）
    if (Total_time <= 0 && ic->duration != AV_NOPTS_VALUE) {
        Total_time = (double)ic->duration / AV_TIME_BASE;
    }

    ffp_notify_msg1(this, FFP_MSG_FIND_STREAM_INFO);
    std::cout << "read_thread FFP_MSG_FIND_STREAM_INFO " << this << std::endl;

    // 6.2 利用av_find_best_stream选择流
    st_index[AVMEDIA_TYPE_VIDEO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                            st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    st_index[AVMEDIA_TYPE_AUDIO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                            st_index[AVMEDIA_TYPE_AUDIO],
                            st_index[AVMEDIA_TYPE_VIDEO],
                            NULL, 0);

    /* open the streams */
    /* 8. 打开视频、音频解码器。在此会打开相应解码器，并创建相应的解码线程。 */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) { // 如果有音频流则打开音频流
        stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) { // 如果有视频流则打开视频流
        ret = stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
    }

    max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    ffp_notify_msg1(this, FFP_MSG_COMPONENT_OPEN);
    std::cout << "read_thread FFP_MSG_COMPONENT_OPEN " << this << std::endl;

    if (video_stream < 0 && audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
               input_filename_);
        ret = -1;
        goto fail;
    }

    ffp_notify_msg1(this, FFP_MSG_PREPARED);
    std::cout << "read_thread FFP_MSG_PREPARED " << this << std::endl;

    while (1) {

        // ========== 处理 Seek 请求 ==========
        if (seek_req) {
            int64_t seek_target = seek_pos;
            int64_t seek_min    = seek_rel > 0 ? seek_target - seek_rel + 2: INT64_MIN;
            int64_t seek_max    = seek_rel < 0 ? seek_target - seek_rel - 2: INT64_MAX;
            // FIXME the +-2 is due to rounding being not done in the correct direction in generation
            //      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, seek_flags);

            if (ret < 0) {
                // ===== FLV ：降级用 AVSEEK_FLAG_ANY 再试一次 =====
                qDebug() << "avformat_seek_file failed, retrying with AVSEEK_FLAG_ANY";
                ret = avformat_seek_file(ic, -1, INT64_MIN, seek_target, INT64_MAX,
                                         AVSEEK_FLAG_ANY);
            }

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", ic->url);
            } else {
                if (audio_stream >= 0)
                    packet_queue_flush(&audioq);
                if (video_stream >= 0)
                    packet_queue_flush(&videoq);

                init_clock(&audclk, &audioq.serial);
                init_clock(&vidclk, &videoq.serial);


                if (is_paused) {
                    ffp_resume_l();
                    is_paused = true;
                    SDL_PauseAudio(1);
                }
            }
            seek_req = 0;
            //queue_attachments_req = 1;
            eof = 0;
            // if (paused)
            //     step_to_next_frame();

        }

        if(abort_request) {
            break;
        }


        // 7.读取媒体数据，得到的是音视频分离后、解码前的数据
        ret = av_read_frame(ic, pkt); // 调用不会释放pkt的数据，需要我们自己去释放packet的数据

        if(ret < 0) { // 出错或者已经读取完毕了
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !eof) { // 读取完毕了

                AVPacket flush_pkt;
                av_init_packet(&flush_pkt);
                flush_pkt.data = NULL;
                flush_pkt.size = 0;

                if (audio_stream >= 0) {
                    flush_pkt.stream_index = audio_stream;
                    packet_queue_put(&audioq, &flush_pkt);
                    qDebug() << "EOF: put nullpacket to audioq";
                }

                if (video_stream >= 0) {
                    flush_pkt.stream_index = video_stream;
                    packet_queue_put(&videoq, &flush_pkt);
                    qDebug() << "EOF: put nullpacket to videoq";
                }

                eof = 1;
            }

            if (ic->pb && ic->pb->error) // io异常 // 退出循环
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 读取完数据了，这里

            continue;    // 继续循环
        } else {
            eof = 0;
        }

        // 插入队列
        if (pkt->stream_index == audio_stream) {
            packet_queue_put(&audioq, pkt);
        }
        else if(pkt->stream_index == video_stream){
            packet_queue_put(&videoq, pkt);
        }else{
            av_packet_unref(pkt); // 不入队列则直接释放数据
        }

    }

    std::cout << __FUNCTION__ << " leave" << std::endl;
    return 0;
fail:
    return -1;
}


Decoder::Decoder()
{
    av_init_packet(&pkt_);
}

Decoder::~Decoder()
{

}

void Decoder::decoder_init(AVCodecContext *avctx, PacketQueue *queue)
{
    avctx_ = avctx;
    queue_ = queue;
}

int Decoder::decoder_start(AVMediaType codec_type, const char *thread_name, void *arg)
{
    // 启用包队列
    packet_queue_start(queue_);

    // 创建线程
    if(AVMEDIA_TYPE_VIDEO == codec_type)
        decoder_thread_ = new std::thread(&Decoder::video_thread, this, arg);
    else if (AVMEDIA_TYPE_AUDIO == codec_type)
        decoder_thread_ = new std::thread(&Decoder::audio_thread, this, arg);
    else
        return -1;
    return 0;
}

void Decoder::decoder_abort(FrameQueue *fq)
{
    packet_queue_abort(queue_);        // 请求退出包队列
    frame_queue_signal(fq);            // 唤醒阻塞的帧队列
    if(decoder_thread_ && decoder_thread_->joinable()) {
        decoder_thread_->join();       // 等待解码线程退出
        delete decoder_thread_;
        decoder_thread_ = NULL;
    }
    packet_queue_flush(queue_);        // 清空packet队列，并释放数据
}


void Decoder::decoder_destroy()
{
    av_packet_unref(&pkt_);
    avcodec_free_context(&avctx_);
}

// 返回值-1: 请求退出
// 0: 解码已经结束了，不再有数据可以读取
// 1: 获取到解码后的frame
int Decoder::decoder_decode_frame(AVFrame *frame)
{
    int ret = AVERROR(EAGAIN);

    for (;;) {
        AVPacket pkt;
        do {

            // 第一个循环 先把codec里的frame 全部读取
            if (queue_->abort_request)    // decoder_abort调用的时候 触发queue_->abort_request为1
                return -1; // 是否请求退出

            if (pkt_serial_ != queue_->serial) {
                // Serial不匹配，说明发生了Seek，丢弃旧包
                // 这里可以选择清空解码器缓存
                avcodec_flush_buffers(avctx_);
                pkt_serial_ = queue_->serial;  // 更新到最新Serial
                continue;
            }

            switch (avctx_->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                ret = avcodec_receive_frame(avctx_, frame);
                //printf("frame pts:%ld, dts:%ld\n", frame->pts, frame->pkt_dts);
                if (ret >= 0) {
                    if (decoder_reorder_pts == -1) {
                        frame->pts = frame->best_effort_timestamp;
                    } else if (!decoder_reorder_pts) {
                        frame->pts = frame->pkt_dts;
                    }
                }
                else {
                    char errStr[256] = { 0 };
                    av_strerror(ret, errStr, sizeof(errStr));
                    //printf("video dec:%s\n", errStr);
                }
                break;

            case AVMEDIA_TYPE_AUDIO:
                ret = avcodec_receive_frame(avctx_, frame);
                if (ret >= 0) {
                    AVRational tb = av_make_q(1, frame->sample_rate);
                    if (frame->pts != AV_NOPTS_VALUE) {
                        // 如果frame->pts正常则先将其从pkt_timebase转成{1, frame->sample_rate}
                        // pkt_timebase实质就是stream->time_base
                        frame->pts = av_rescale_q(frame->pts, avctx_->pkt_timebase, tb);
                    }else{
                        qDebug()<<"audio frame error";
                    }
                    //else if (d->next_pts != AV_NOPTS_VALUE) {
                    //     // 如果frame->pts不正常则使用上一帧更新的next_pts和next_pts_tb
                    //     // 转成{1, frame->sample_rate}
                    //     frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                    // }

                }
                else {
                    char errStr[256] = { 0 };
                    av_strerror(ret, errStr, sizeof(errStr));
                    //printf("audio dec:%s\n", errStr);
                }
                break;
            }

            // 1.3. 检查解码是否已经结束，解码结束返回0
            if (ret == AVERROR_EOF) {
                printf("avcodec_flush_buffers %s(%d)\n", __FUNCTION__, __LINE__);
                avcodec_flush_buffers(avctx_);
                return 0;
            }
            // 1.4. 正常解码返回1
            if (ret >= 0)
                return 1;    // 获取到一帧frame

        } while (ret != AVERROR(EAGAIN));    // 1.5 没帧可读时ret返回EAGIN，需要继续送packet



        do {

            int old_serial = pkt_serial_;

            if (packet_queue_get(queue_, &pkt, 1, &pkt_serial_) < 0)
                return -1;

            if (old_serial != pkt_serial_) {
                avcodec_flush_buffers(avctx_);
            }

            if (queue_->serial == pkt_serial_)
                break;

            // if (pkt_serial_ != queue_->serial) {
            //     avcodec_flush_buffers(avctx_); // 必须清空解码器上下文缓存
            //     pkt_serial_ = queue_->serial;
            //     av_packet_unref(&pkt);
            // }

            av_packet_unref(&pkt);

        } while (1);


        if (avcodec_send_packet(avctx_, &pkt) == AVERROR(EAGAIN)) {
            av_log(avctx_, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
            // 先暂存这个pkt
        }
        av_packet_unref(&pkt);    // 一定要自己去释放音视频数据
    }
}


int Decoder::get_video_frame_l(AVFrame *frame,void *arg)
{
    int got_picture;

    FFPlayer *is = (FFPlayer*) arg;

    // 1. 获取解码后的视频帧
    if ((got_picture = decoder_decode_frame(frame)) < 0) {
        return -1; // 返回-1意味着要退出解码线程，所以要分析decoder_decode_frame什么情况下返回-1
    }

    if (got_picture) {
        double dpts = NAN;
        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        // 新增：启动阶段检测（前 100 帧或前 10 秒）
        static int startup_frames = 0;
        static bool startup_phase = true;
        startup_frames++;
        if (startup_frames > 100) startup_phase = false;

        // 放宽丢帧条件
        if (is->framedrop > 0 || (is->framedrop && is->get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - is->get_master_clock();

                // 启动阶段：差距 > 5 秒时不丢帧，允许追赶
                bool should_drop = !isnan(diff) &&
                                   fabs(diff) < AV_NOSYNC_THRESHOLD &&
                                   diff - is->frame_last_filter_delay < 0;

                if (startup_phase && fabs(diff) > 5.0) {
                    should_drop = false;  // 启动阶段大差距不丢
                    qDebug() << "Startup phase: keeping frame despite diff =" << diff;
                }

                if (should_drop) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

int Decoder::queue_picture(FrameQueue *fq, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    Frame *vp;

    if (!(vp = frame_queue_peek_writable(fq))) // 检测队列是否有可写空间
        return -1;    // 请求退出则返回-1

    // 执行到这步说已经获取到了可写入的Frame
    //    vp->sar = src_frame->sample_aspect_ratio;
    //    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;

    vp->pos = pos;
    vp->serial = serial;

    av_frame_move_ref(vp->frame, src_frame); // 将src中所有数据转移到dst中，并复位src。
    frame_queue_push(fq);    // 更新写索引位置
    return 0;
}

int Decoder::audio_thread(void *arg)
{
    std::cout << __FUNCTION__ << " into " << std::endl;
    FFPlayer *is = (FFPlayer *)arg;
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int got_frame = 0;
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM);

    do {
        if ((got_frame = decoder_decode_frame(frame)) < 0)
            goto the_end;

        if (got_frame) {
            // 获取可写Frame

            if (!(af = frame_queue_peek_writable(&is->sampq)))
                goto the_end;


            af->serial = pkt_serial_;

            // 正确的时间基转换
            //AVRational src_tb = avctx_->pkt_timebase;  // packet 的时间基
            if (frame->pts != AV_NOPTS_VALUE) {
                // 先转成秒，这是最安全的做法
                //af->pts = frame->pts * av_q2d(src_tb);
                af->pts = frame->pts / (double)frame->sample_rate;
            } else {
                af->pts = NAN;
            }

            // 设置 duration（用于音频同步）
            AVRational tb;
            tb.num = frame->nb_samples;
            tb.den = frame->sample_rate;
            af->duration = av_q2d(tb);

            // 在得到 af->pts 之后、入队之前
            if (isnan(is->audio_pts_offset)) {
                is->audio_pts_offset = af->pts;   // 记录第一个音频帧的 PTS
            }
            af->pts -= is->audio_pts_offset;      // 对齐到 0

            av_frame_move_ref(af->frame, frame);
            frame_queue_push(&is->sampq);
            //printf("audio_thread: pushed frame, pts=%f\n", af->pts);  // 加这行

        }
    } while (got_frame >= 0 || got_frame == AVERROR(EAGAIN));

the_end:
    av_frame_free(&frame);
    return ret;
}

int Decoder::video_thread(void *arg)
{
    std::cout << __FUNCTION__ << " into " << std::endl;
    FFPlayer *is = (FFPlayer *)arg;
    AVFrame *frame = av_frame_alloc(); // 分配解码帧
    double pts;               // pts
    double duration;          // 帧持续时间
    int ret;
    //1 获取stream timebase
    //实际上这里用 avctx_->pkt_timebase 去算也是可以的，因为两个值一样是同一个东西
    //AVRational tb = is->video_st->time_base; // 获取stream timebase

    AVRational tb = avctx_->pkt_timebase;

    //2 获取帧率，以便计算每帧picture的duration
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    if (!frame)
        return AVERROR(ENOMEM);

    for (;;) {  // 循环取出视频解码的帧数据
        // 3 获取解码后的视频帧
        ret = get_video_frame_l(frame,arg);
        if (ret < 0)
            goto the_end;   //解码结束，什么时候会结束
        if (!ret)           //没有解码得到画面，什么情况下会得不到解后的帧
            continue;

        // 4 计算帧持续时间和换算pts值为秒
        // 1/帧率 = duration 单位秒，没有帧率时则设置为0，有帧率计算出帧间隔
        AVRational frame_duration;
        frame_duration.num = frame_rate.den;
        frame_duration.den = frame_rate.num;
        duration = (frame_rate.num && frame_rate.den ? av_q2d(frame_duration) : 0);
        // 根据AVStream timebase计算出pts值，单位为秒
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

        // 在计算 pts 之后、调用 queue_picture 之前
        if (isnan(is->video_pts_offset)) {
            is->video_pts_offset = pts;       // 记录第一个视频帧的 PTS
        }
        pts -= is->video_pts_offset;          // 对齐到 0

        // 5 将解码后的视频帧插入队列
        ret = queue_picture(&is->pictq, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial_);

        // 6 释放frame对应的数据
        av_frame_unref(frame);

        if (ret < 0) // 返回值小于0则退出线程
            goto the_end;
    }

the_end:
    std::cout << __FUNCTION__ << " leave " << std::endl;
    av_frame_free(&frame);
    return 0;
}

void FFPlayer::AddVideoRefreshCallback(std::function<int (const Frame *)> callback)
{
    video_refresh_callback_ = callback;
}

double FFPlayer::compute_target_delay_l(double delay, Frame *vp)
{
    double sync_threshold, diff = 0;

    // 先按速率缩短基础帧间延迟
    if (playback_rate_ > 0.01)
        delay /= playback_rate_;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */

        //qDebug()<<"get_master_clock:"<<get_master_clock();

        diff = get_clock(&vidclk) - get_master_clock();


        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
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

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
           delay, -diff);

    return delay;
}

double FFPlayer::vp_duration(Frame *vp, Frame *nextvp)
{
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > max_frame_duration)
            return vp->duration;
        else
            return duration;
    } else {
        return 0.0;
    }
}

void FFPlayer::step_to_next_frame()
{

    Frame *vp = NULL;

    if (frame_queue_nb_remaining(&pictq) == 0) {
        // 什么都不用做，可以直接退出了
        return;
    }

    vp = frame_queue_peek(&pictq);

    if(video_refresh_callback_)
        video_refresh_callback_(vp);
    else
        std::cout << __FUNCTION__ << " step_to_next_frame NULL" << std::endl;

    frame_queue_next(&pictq);    // 当前vp帧出队
}

int64_t FFPlayer::getTotalTime()
{

    if(ic)
        return Total_time;
    else
        return 0;
}

void FFPlayer::setVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    audio_volume = volume;
}

int FFPlayer::getVolume() const
{
    return audio_volume;
}

void FFPlayer::setMute(bool mute)
{
    if (mute && !audio_muted) {
        volume_before_mute = audio_volume;
        qDebug()<<"make mute success";
        audio_volume = 0;
        audio_muted = true;
    } else if (!mute && audio_muted) {
        audio_volume = volume_before_mute;
        audio_muted = false;
    }
}

bool FFPlayer::isMuted() const
{
    return audio_muted;
}

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01    // 每帧休眠10ms

int FFPlayer::video_refresh_thread()
{
    double remaining_time = 0.0;
    while (!abort_request) {
        if (remaining_time > 0.0)
            av_usleep((int)(int64_t)(remaining_time * 1000000.0));
        if (is_paused) {
            remaining_time = 0.05;  // 50ms检查一次
        } else {
            remaining_time = REFRESH_RATE;  // 0.01
        }
        video_refresh(&remaining_time);
    }
    std::cout << __FUNCTION__ << " leave" << std::endl;
    return 0;
}

void FFPlayer::video_refresh(double *remaining_time)
{
    //Frame *vp = NULL;
    // 目前我们先是只有队列里面有视频帧可以播放，就先播放出来
    // 判断有没有视频画面
    Frame *vp = NULL, *lastvp= NULL;

    if(video_st) {
retry:
        if (frame_queue_nb_remaining(&pictq) == 0) {
            // 什么都不用做，可以直接退出了
            return;
        }

        if (is_paused) {
            //qDebug()<<"Stop return video_refresh:"<<is_paused;
            *remaining_time = 0.1;  // 100ms后继续检查
            return;  // 不显示新帧
        }

        double last_duration, duration, delay,time;

        // // 能跑到这里说明帧队列不为空，肯定有frame可以读取
        vp = frame_queue_peek(&pictq);          // 读取待显示帧
        lastvp = frame_queue_peek_last(&pictq); // 获取上一帧（用于计算延迟）

        // //如果当前帧和我队列的序列不一致就重新走这遍流程
        if (vp->serial != videoq.serial) {
            frame_queue_next(&pictq);
            goto retry;
            qDebug()<<"jump retry serial";
        }

        if (lastvp->serial != vp->serial)
            frame_timer = av_gettime_relative() / 1000000.0;

        // if (is_paused)
        //     goto display;

        // //这里是计算是否快了
        last_duration = vp_duration(lastvp, vp);
        delay = compute_target_delay_l(last_duration,vp);
        time = av_gettime_relative()/1000000.0;

        if (time < frame_timer + delay) {
            *remaining_time = FFMIN(frame_timer + delay - time, *remaining_time);
            return;
        }

        frame_timer += delay;
        if (delay > 0 && time - frame_timer > AV_SYNC_THRESHOLD_MAX)
            frame_timer = time;

        if (!isnan(vp->pts)) {
            set_clock(&vidclk, vp->pts,vp->serial);  // 更新视频时钟为当前帧pts

            if (progress_callback_) {
                progress_callback_(vp->pts);  // 传递当前PTS（秒）
            }



        }

        // 6. 视频慢了或正好：显示当前帧
        // 检查是否需要丢帧追赶（多帧落后）
        // 如果落后超过一帧，直接丢弃
        if (frame_queue_nb_remaining(&pictq) > 1) {
            Frame *nextvp = frame_queue_peek_next(&pictq);
            duration = vp_duration(vp, nextvp);

            if(!step && (framedrop>0 || (framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) && time > frame_timer + duration){
                frame_drops_late++;
                frame_queue_next(&pictq);
                //qDebug()<<"video refresh drops:";
                goto retry;
            }
        }

        // qDebug()<<"audio_clock:"<<audio_clock
        //          <<"video_clock"<<vp->pts
        //          <<"diff"<<audio_clock - vp->pts;


        //std::cout << __FUNCTION__ << ": vp->pts:" << vp->pts << " - af->pts:" << get_master_clock();

display:

        // 刷新显示
        if(video_refresh_callback_)
            video_refresh_callback_(vp);
        else
            std::cout << __FUNCTION__ << " video_refresh_callback_ NULL" << std::endl;

        frame_queue_next(&pictq);    // 当前vp帧出队

    }
}

int FFPlayer::get_master_sync_type()
{
    if (av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;  /* 如果没有视频成分则使用 audio master */
    } else if (av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else if (video_st)
            return AV_SYNC_VIDEO_MASTER;   // 只有音频的存在（注释笔误，应为“只有视频的存在”）
        else
            return AV_SYNC_UNKNOW_MASTER;
    }
}

double FFPlayer::get_master_clock()
{
    double val;

    switch (get_master_sync_type()) {
    case AV_SYNC_VIDEO_MASTER:
        val = get_clock(&vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = get_clock(&audclk);
        break;
    default:
        val = get_clock(&audclk);
        break;
    }

    return val;
}

int FFPlayer::ffp_pause_l()
{
    if (is_paused) return 0;

    is_paused = true;

    // 暂停SDL音频（硬件暂停，缓冲区保留）
    SDL_PauseAudio(1);

    // 记录暂停时的时间偏移
    // frame_timer是基于系统时间的，暂停期间系统时间继续走，需要补偿
    frame_timer_pause_offset = av_gettime_relative() / 1000000.0 - frame_timer;

    qDebug() << "Paused at frame_timer:" << frame_timer
             << "offset:" << frame_timer_pause_offset;

    return 0;
}

int FFPlayer::ffp_resume_l()
{
    if (!is_paused) return 0;

    // 恢复frame_timer基准（减去暂停期间流逝的时间）
    frame_timer = av_gettime_relative() / 1000000.0 - frame_timer_pause_offset;

    // 恢复SDL音频
    SDL_PauseAudio(0);

    is_paused = false;

    qDebug() << "Resumed, frame_timer adjusted to:" << frame_timer;

    return 0;
}

void FFPlayer::SetProgressCallback(std::function<void(double)> callback)
{
    progress_callback_ = callback;
}

void FFPlayer::setPlaybackRate(double rate)
{
    if (rate < 0.1) rate = 0.1;
    playback_rate_ = rate;

    if (soundtouch_initialized_) {
        soundTouch_.setTempo(rate);
        soundTouch_.flush();
    }

    frame_timer = av_gettime_relative() / 1000000.0;

    // 切速率时强制让 audclk 和 vidclk 重新对齐
    // 用当前 audio_clock 直接重置 audclk，消除残留偏差
    if (!isnan(audio_clock)) {
        set_clock_at(&audclk,
                     audio_clock,
                     auddec.pkt_serial_,
                     av_gettime_relative() / 1000000.0);
    }
    init_clock(&vidclk, &videoq.serial);  // vidclk 重新从下一帧开始对齐

    qDebug() << "setPlaybackRate:" << rate;
}
