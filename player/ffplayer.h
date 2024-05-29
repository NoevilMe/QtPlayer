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

class VideoPlayer;
class AudioPlayer;

class FFPlayer {
public:
    FFPlayer();
    ~FFPlayer();

    bool Start();
    void Stop();

    void SetMediaSource(MediaSource media);
    void SetAudioDeviceFormat(AudioDeviceFormat fmt);

    void SetFrameCallback(const std::function<void(AVFrame *)> &cb) {
        video_frame_cb_ = cb;
    }

    void
    SetAudioFrameCallback(const std::function<void(char *, int, double)> &cb) {
        audio_frame_cb_ = cb;
    }

    void SetAudioClockCallback(const std::function<double()> &cb) {
        audio_clock_cb_ = cb;
    }

    void PlayVideoFrame(AVFrame *frame, double clock);
    void PlayAudioFrame(char *data, int length, double clock);

protected:
    bool ResampleFormatValid() const;

    bool InitInputContext();
    bool InitInputCodec();

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

    static int InterruptCallback(void *context);

    void ThreadFunc();

protected:
    MediaSource media_source_;
    ResampleFormat resample_fmt_;

    AvFunctionInterrupt interrupt_;

    // 视频硬件加速设备
    AVHWDeviceType hwtype_;

    // 输入
    AVFormatContext *fmt_ctx_ = nullptr;
    // video

    std::unique_ptr<VideoPlayer> video_player_;
    std::unique_ptr<AudioPlayer> audio_player_;

    // audio
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
    std::function<void(AVFrame *)> video_frame_cb_;
    std::function<void(char *, int, double)> audio_frame_cb_;
    std::function<double()> audio_clock_cb_;
    std::shared_ptr<spdlog::logger> logger_;
};

class AVPlayer {
public:
    AVPlayer(int index, AVStream *);
    virtual ~AVPlayer();

    int index() const { return index_; }
    AVCodecID CodecID() const;
    AVCodecParameters *CodecPar() const;
    // 时间基，只能从流中读取。解码器的时间不能用于计算clock。
    AVRational TimeBase();

    void set_codec(const AVCodec *c);
    const AVCodec *codec() const { return codec_; }

    void ResetDecodeContext();

    virtual void LogInput() = 0;
    virtual void LogHw() {}
    virtual bool InitDecodeContext() = 0;
    virtual bool HandleFrame(AVPacket *pkt) = 0;

protected:
    int index_;
    AVStream *stream_;
    const AVCodec *codec_ = nullptr;
    AVCodecContext *decode_ctx_ = nullptr;

    AVFrame *decoded_frame_ = nullptr;

    std::shared_ptr<spdlog::logger> logger_;
};

class VideoPlayer : public AVPlayer {
public:
    VideoPlayer(int index, AVStream *stream);
    ~VideoPlayer();

    int Fps();

    bool InitHWDeviceContext(const AVCodec *codec, AVHWDeviceType hwtype);
    bool InitSwsContext();

    void ResetHWDeviceContext();
    void ResetSwsContext();

    void SetFrameCallback(const std::function<void(AVFrame *, double)> &cb) {
        frame_cb_ = cb;
    }

private:
    static enum AVPixelFormat GetHwFormat(AVCodecContext *ctx,
                                          const enum AVPixelFormat *pix_fmts);

    // AVPlayer interface
public:
    void LogInput() override;
    void LogHw() override;
    bool InitDecodeContext() override;
    bool HandleFrame(AVPacket *pkt) override;

private:
    // 视频硬件加速设备
    AVHWDeviceType hwtype_ = AV_HWDEVICE_TYPE_NONE;
    AVBufferRef *hw_device_ctx_ = nullptr;
    AVPixelFormat hw_pix_fmt_ = AVPixelFormat::AV_PIX_FMT_NONE;

    // 图像转换
    AVPixelFormat sws_fmt_ = AV_PIX_FMT_YUV420P;
    int sws_width_ = 0;
    int sws_height_ = 0;
    SwsContext *sws_ctx_ = nullptr;

    // 映射还是下载
    bool map_hw_frame_ = true;

    long long ts_start_ = 0;
    long long ts_decode_ = 0;
    long long ts_hw_ = 0;  // transfer or map
    long long ts_sws_ = 0; // sws_scale
    long long ts_cb_ = 0;

    std::function<void(AVFrame *, double)> frame_cb_;
};

class AudioPlayer : public AVPlayer {
public:
    AudioPlayer(int index, AVStream *stream);
    ~AudioPlayer();

    void set_resample_format(ResampleFormat fmt);

    // AVPlayer interface
    void LogInput() override;
    bool InitDecodeContext() override;
    bool HandleFrame(AVPacket *pkt) override;

    bool InitSwrContext();
    void ResetSwrContext();

    void SetFrameCallback(const std::function<void(char *, int, double)> &cb) {
        frame_cb_ = cb;
    }

private:
    bool ResampleFormatValid() const;

private:
    ResampleFormat resample_fmt_;
    SwrContext *swr_ctx_ = nullptr;

    std::function<void(char *, int, double)> frame_cb_;
};

#endif // FFPLAYER_H
