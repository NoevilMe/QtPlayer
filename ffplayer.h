#ifndef FFPLAYER_H
#define FFPLAYER_H

#include "util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

const AVCodecHWConfig *AvUtilGetHwConfig(const AVCodec *codec,
                                         AVHWDeviceType hwtype);

struct AvFunctionInterrupt {
    long long func_start_timestamp = 0;
    long long func_end_timestamp = 0;
    bool interrupted = false;
};

class FFPlayer {
public:
    FFPlayer();
    ~FFPlayer();

    bool Start();

protected:
    bool InitInputContext();
    bool InitHWDeviceContext(const AVCodec *codec, bool get_hw_type=false);
    bool InitInputDecodeContext(const AVCodec *dec);

    bool HandleInputFrame(AVPacket *pkt);

    static enum AVPixelFormat GetFormat(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts);
    static int InterruptCallback(void *context);

    void ThreadFunc();

protected:
    AvFunctionInterrupt interrupt_;

    AVHWDeviceType hwtype_ = AV_HWDEVICE_TYPE_VAAPI;
    // 硬件加速设备
    AVBufferRef *hw_device_ctx_ = nullptr;

    AVFormatContext *input_fmt_ctx_ = nullptr;
    AVStream *input_video_stream_ = nullptr;
    AVStream *input_audio_stream = nullptr;

    AVCodecContext *input_decode_ctx_ = nullptr;
     AVFrame *decode_frame_ = nullptr;

    AVPixelFormat hw_pix_fmt_ = AVPixelFormat::AV_PIX_FMT_NONE;

    std::atomic_bool running_;
    std::thread thd_;
    std::shared_ptr<spdlog::logger> logger_;
};

#endif // FFPLAYER_H
