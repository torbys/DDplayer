#include "thumbnail_extractor.h"
#include <QThread>
#include <QPixmap>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

ThumbnailExtractor::ThumbnailExtractor(QObject *parent)
    : QObject(parent)
{
}

ThumbnailExtractor::~ThumbnailExtractor()
{
}

QImage ThumbnailExtractor::extractFirstFrame(const QString &filePath, int maxWidth, int maxHeight)
{
    QImage result;
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwsContext *swsCtx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;

    do {
        if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) != 0)
            break;

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
            break;

        int videoStreamIdx = -1;
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIdx = static_cast<int>(i);
                break;
            }
        }

        if (videoStreamIdx < 0)
            break;

        const AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[videoStreamIdx]->codecpar->codec_id);
        if (!codec)
            break;

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx)
            break;

        if (avcodec_parameters_to_context(codecCtx, fmtCtx->streams[videoStreamIdx]->codecpar) < 0)
            break;

        if (avcodec_open2(codecCtx, codec, nullptr) < 0)
            break;

        pkt = av_packet_alloc();
        frame = av_frame_alloc();
        if (!pkt || !frame)
            break;

        while (av_read_frame(fmtCtx, pkt) >= 0) {
            if (pkt->stream_index == videoStreamIdx) {
                int ret = avcodec_send_packet(codecCtx, pkt);
                if (ret >= 0 && avcodec_receive_frame(codecCtx, frame) >= 0) {
                    av_packet_unref(pkt);
                    break;
                }
            }
            av_packet_unref(pkt);
        }

        if (frame->width <= 0 || frame->height <= 0)
            break;

        double aspectRatio = static_cast<double>(frame->width) / frame->height;
        int dstWidth, dstHeight;
        if (aspectRatio > static_cast<double>(maxWidth) / maxHeight) {
            dstWidth = maxWidth;
            dstHeight = static_cast<int>(maxWidth / aspectRatio);
        } else {
            dstHeight = maxHeight;
            dstWidth = static_cast<int>(maxHeight * aspectRatio);
        }

        swsCtx = sws_getContext(frame->width, frame->height,
                                static_cast<AVPixelFormat>(frame->format),
                                dstWidth, dstHeight,
                                AV_PIX_FMT_RGB24,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!swsCtx)
            break;

        AVFrame *rgbFrame = av_frame_alloc();
        if (!rgbFrame)
            break;

        rgbFrame->width = dstWidth;
        rgbFrame->height = dstHeight;
        rgbFrame->format = AV_PIX_FMT_RGB24;
        av_image_alloc(rgbFrame->data, rgbFrame->linesize, dstWidth, dstHeight, AV_PIX_FMT_RGB24, 16);

        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
                  rgbFrame->data, rgbFrame->linesize);

        result = QImage(dstWidth, dstHeight, QImage::Format_RGB888);
        for (int y = 0; y < dstHeight; y++) {
            memcpy(result.scanLine(y), rgbFrame->data[0] + y * rgbFrame->linesize[0], dstWidth * 3);
        }

        av_freep(&rgbFrame->data[0]);
        av_frame_free(&rgbFrame);

    } while (false);

    if (swsCtx) sws_freeContext(swsCtx);
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (fmtCtx) avformat_close_input(&fmtCtx);

    return result;
}

void ThumbnailExtractor::extractAsync(const QString &filePath, int maxWidth, int maxHeight)
{
    QtConcurrent::run([this, filePath, maxWidth, maxHeight]() {
        QImage img = extractFirstFrame(filePath, maxWidth, maxHeight);
        emit thumbnailReady(filePath, img);
    });
}
