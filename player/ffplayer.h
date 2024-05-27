#ifndef FFPLAYER_H
#define FFPLAYER_H

#include "av_def.h"

#include "util/util.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct ResampleFormat {
    int sample_rate = 0;
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
    int channel_count = 0;
};

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
    void Stop();

    void SetMediaSource(MediaSource media);
    void SetAudioDeviceFormat(AudioDeviceFormat fmt);

    void SetFrameCallback(const std::function<void(AVFrame *)> &cb) {
        frame_cb_ = cb;
    }

    void SetAudioFrameCallback(
        const std::function<void(char *, int, long long)> &cb) {
        audio_frame_cb_ = cb;
    }

protected:
    bool ResampleFormatValid() const;

    bool InitInputContext();
    bool InitInputCodec();
    bool InitHWDeviceContext(const AVCodec *codec, AVHWDeviceType hwtype,
                             bool get_hw_type = false);
    bool InitDecodeContext();
    bool InitSwrContext();
    bool InitSwsContext();

    void ResetInputContext();
    void ResetDecodeContext();
    void ResetHWDeviceContext();
    void ResetSwsContext();
    void ResetSwrContext();

    bool HandleVideoFrame(AVPacket *pkt);
    bool HandleAudioFrame(AVPacket *pkt);

    static enum AVPixelFormat GetFormat(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts);
    static int InterruptCallback(void *context);

    void ThreadFunc();

protected:
    MediaSource media_source_;
    ResampleFormat resample_fmt_;

    AvFunctionInterrupt interrupt_;

    // 视频硬件加速设备
    AVHWDeviceType hwtype_;
    AVBufferRef *hw_device_ctx_ = nullptr;
    AVPixelFormat hw_pix_fmt_ = AVPixelFormat::AV_PIX_FMT_NONE;
    bool map_hw_frame_ = true;

    // 输入
    AVFormatContext *fmt_ctx_ = nullptr;
    // video
    int video_index_ = -1;
    AVStream *video_stream_ = nullptr;

    const AVCodec *video_codec_ = nullptr;
    AVCodecContext *video_decode_ctx_ = nullptr;
    int64_t video_decode_dts_ = 0;
    AVFrame *video_frame_ = nullptr;
    long long video_pts = 0;

    AVPixelFormat sws_fmt_ = AV_PIX_FMT_YUV420P;
    int sws_width_ = 0;
    int sws_height_ = 0;
    SwsContext *sws_ctx_ = nullptr;

    // audio
    int audio_index_ = -1;
    AVStream *audio_stream_ = nullptr;
    const AVCodec *audio_codec_ = nullptr;
    AVCodecContext *audio_decode_ctx_ = nullptr;
    int64_t audio_decode_dts_ = 0;
    AVFrame *audio_frame_ = nullptr;
    long long audio_pts = 0;

    SwrContext *swr_ctx_ = nullptr;

    long long ts_get_ = 0;
    long long ts_decode_ = 0;
    long long ts_hw_ = 0;  // transfer or map
    long long ts_sws_ = 0; // sws_scale
    long long ts_cb_ = 0;

    std::atomic_bool running_;
    std::thread thd_;
    std::function<void(AVFrame *)> frame_cb_;
    std::function<void(char *, int, long long)> audio_frame_cb_;
    std::shared_ptr<spdlog::logger> logger_;
};

#endif // FFPLAYER_H
