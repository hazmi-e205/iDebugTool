#include "windowsthumbnail.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTransform>

#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#endif

QPixmap LoadWindowsShellThumbnail(const QString& localPath, const QSize& size)
{
#ifndef Q_OS_WIN
    Q_UNUSED(localPath);
    Q_UNUSED(size);
    return QPixmap();
#else
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninitialize = SUCCEEDED(initResult);

    QString nativePath = QDir::toNativeSeparators(localPath);

    IShellItemImageFactory *factory = nullptr;
    HRESULT result = SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(nativePath.utf16()), nullptr, IID_PPV_ARGS(&factory));
    if (FAILED(result) || !factory) {
        if (shouldUninitialize)
            CoUninitialize();
        return QPixmap();
    }

    SIZE requestedSize = { size.width(), size.height() };
    HBITMAP bitmap = nullptr;
    result = factory->GetImage(requestedSize, SIIGBF_BIGGERSIZEOK | SIIGBF_RESIZETOFIT, &bitmap);
    factory->Release();

    QPixmap pixmap;
    if (SUCCEEDED(result) && bitmap) {
        BITMAP bitmapInfo;
        if (GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) > 0) {
            QImage image(bitmapInfo.bmWidth, bitmapInfo.bmHeight, QImage::Format_ARGB32);
            BITMAPINFO dibInfo;
            ZeroMemory(&dibInfo, sizeof(dibInfo));
            dibInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            dibInfo.bmiHeader.biWidth = bitmapInfo.bmWidth;
            dibInfo.bmiHeader.biHeight = -bitmapInfo.bmHeight;
            dibInfo.bmiHeader.biPlanes = 1;
            dibInfo.bmiHeader.biBitCount = 32;
            dibInfo.bmiHeader.biCompression = BI_RGB;

            HDC dc = GetDC(nullptr);
            if (dc) {
                if (GetDIBits(dc, bitmap, 0, bitmapInfo.bmHeight, image.bits(), &dibInfo, DIB_RGB_COLORS) != 0)
                    pixmap = QPixmap::fromImage(image);
                ReleaseDC(nullptr, dc);
            }
        }
        DeleteObject(bitmap);
    }

    if (shouldUninitialize)
        CoUninitialize();
    return pixmap;
#endif
}

namespace {

QImage ConvertFrameToImage(AVFrame* frame, int targetWidth)
{
    if (frame->width <= 0 || frame->height <= 0)
        return QImage();

    int outW = targetWidth > 0 ? targetWidth : frame->width;
    int outH = static_cast<int>(static_cast<qint64>(frame->height) * outW / frame->width);
    if (outH < 1) outH = 1;
    if (outH & 1) ++outH;
    if (outW & 1) ++outW;

    SwsContext* sws = sws_getContext(
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        outW, outH, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws)
        return QImage();

    QImage image(outW, outH, QImage::Format_RGB888);
    uint8_t* dst[4] = { image.bits(), nullptr, nullptr, nullptr };
    int dstStride[4] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, dstStride);
    sws_freeContext(sws);
    return image;
}

double DisplayRotationDegrees(AVStream* stream)
{
    if (!stream)
        return 0.0;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(60, 15, 100)
    for (int i = 0; i < stream->codecpar->nb_coded_side_data; ++i) {
        const AVPacketSideData& sd = stream->codecpar->coded_side_data[i];
        if (sd.type == AV_PKT_DATA_DISPLAYMATRIX && sd.size >= 9 * sizeof(int32_t)) {
            double rot = av_display_rotation_get(reinterpret_cast<const int32_t*>(sd.data));
            if (!std::isnan(rot))
                return rot;
        }
    }
#else
    for (int i = 0; i < stream->nb_side_data; ++i) {
        const AVPacketSideData& sd = stream->side_data[i];
        if (sd.type == AV_PKT_DATA_DISPLAYMATRIX && sd.size >= 9 * sizeof(int32_t)) {
            double rot = av_display_rotation_get(reinterpret_cast<const int32_t*>(sd.data));
            if (!std::isnan(rot))
                return rot;
        }
    }
#endif
    return 0.0;
}

} // namespace

QPixmap LoadFfmpegVideoThumbnail(const QString& localPath, const QSize& size)
{
    if (!QFileInfo::exists(localPath)) {
        qWarning() << "[thumb] missing video:" << localPath;
        return QPixmap();
    }

    const QByteArray pathUtf8 = QDir::toNativeSeparators(localPath).toUtf8();
    const int targetWidth = size.width() > 0 ? size.width() : 256;

    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, pathUtf8.constData(), nullptr, nullptr) < 0) {
        qWarning() << "[thumb] avformat_open_input failed for" << localPath;
        return QPixmap();
    }

    QPixmap result;
    do {
        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            qWarning() << "[thumb] avformat_find_stream_info failed";
            break;
        }

        const AVCodec* decoder = nullptr;
        int streamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
        if (streamIdx < 0 || !decoder) {
            qWarning() << "[thumb] no video stream";
            break;
        }

        AVStream* stream = fmtCtx->streams[streamIdx];
        AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
        if (!codecCtx)
            break;

        bool opened = false;
        do {
            if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0) {
                qWarning() << "[thumb] avcodec_parameters_to_context failed";
                break;
            }
            codecCtx->thread_count = 1;
            if (avcodec_open2(codecCtx, decoder, nullptr) < 0) {
                qWarning() << "[thumb] avcodec_open2 failed";
                break;
            }
            opened = true;

            if (stream->duration > 0) {
                int64_t oneSecondInStreamTb = av_rescale_q(AV_TIME_BASE, AV_TIME_BASE_Q, stream->time_base);
                if (oneSecondInStreamTb < stream->duration)
                    av_seek_frame(fmtCtx, streamIdx, oneSecondInStreamTb, AVSEEK_FLAG_BACKWARD);
            }

            AVPacket* pkt = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();
            QImage decoded;
            int framesTried = 0;
            const int kMaxFrames = 60;

            while (decoded.isNull() && framesTried < kMaxFrames && av_read_frame(fmtCtx, pkt) >= 0) {
                if (pkt->stream_index != streamIdx) {
                    av_packet_unref(pkt);
                    continue;
                }
                int sendRet = avcodec_send_packet(codecCtx, pkt);
                av_packet_unref(pkt);
                if (sendRet < 0)
                    continue;

                while (true) {
                    int recvRet = avcodec_receive_frame(codecCtx, frame);
                    if (recvRet == AVERROR(EAGAIN) || recvRet == AVERROR_EOF)
                        break;
                    if (recvRet < 0)
                        break;
                    ++framesTried;
                    decoded = ConvertFrameToImage(frame, targetWidth);
                    av_frame_unref(frame);
                    if (!decoded.isNull())
                        break;
                }
            }

            if (decoded.isNull()) {
                // Flush decoder.
                avcodec_send_packet(codecCtx, nullptr);
                while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                    decoded = ConvertFrameToImage(frame, targetWidth);
                    av_frame_unref(frame);
                    if (!decoded.isNull())
                        break;
                }
            }

            av_frame_free(&frame);
            av_packet_free(&pkt);

            if (!decoded.isNull()) {
                double rot = DisplayRotationDegrees(stream);
                if (std::abs(rot) > 0.5) {
                    QTransform t;
                    t.rotate(-rot);
                    decoded = decoded.transformed(t, Qt::SmoothTransformation);
                }
                result = QPixmap::fromImage(decoded);
            } else {
                qWarning() << "[thumb] no frame decoded from" << localPath;
            }
        } while (false);

        if (opened)
            avcodec_free_context(&codecCtx);
        else if (codecCtx)
            avcodec_free_context(&codecCtx);
    } while (false);

    avformat_close_input(&fmtCtx);
    return result;
}
