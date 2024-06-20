#ifndef FFPLAYER_H
#define FFPLAYER_H

#include "av_def.h"
#include "util/util.h"

#include <list>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct AvFunctionInterrupt {
    long long func_start_timestamp = 0;
    long long func_end_timestamp = 0;
    bool interrupted = false;
};

enum class MediaType {
    kMediaNone = 0,
    kMediaFile = 1,
    kMediaNetwork,
    kMediaCapture
};

struct MediaSource {
    MediaType type = MediaType::kMediaNone;
    std::string src;
};

struct AudioFormat {
    AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
    int sample_rate = 0;
    int channel_count = 0;
};

struct VideoFormat {
    int width = 0;
    int height = 0;
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    AVRational sample_aspect_ratio = {0, 0};
    AVColorPrimaries color_primaries = AVCOL_PRI_UNSPECIFIED;
};

class VideoPlayer;
class AudioPlayer;

class FFPlayer {
public:
    FFPlayer();
    ~FFPlayer();

    // 设置源，open前
    void SetMediaSource(MediaSource media);

    // 先打开，检测视频流、音频流
    bool Open();
    // 是否打开
    bool is_open() const { return is_open_; }

    // 含有视频流、音频流
    bool HasVideo() const;
    bool HasAudio() const;
    // 获取音频参数，用于重采样
    bool GetAudioFormat(AudioFormat *out_fmt);
    // 获取视频参数，用于窗口调整
    bool GetVideoFormat(VideoFormat *out_fmt);

    // 获取总时长
    double GetTotalSeconds();
    long long GetDuration();
    // 获取已经播放的时间
    double GetClock();

    // 设置音频重采样参数，Play前
    void SetAudioResampleFormat(AudioFormat fmt);
    void SetAudioDeviceFormat(AudioDeviceFormat fmt);

    // 再播放
    bool Play();
    // 是否播放
    bool IsPlaying() { return running_.load(); }

    bool Pause();
    bool isPaused() { return paused_.load(); }

    // 停止播放
    void Stop();

    void SetVideoFrameCallback(const std::function<void(AVFrame *)> &cb) {
        video_frame_cb_ = cb;
    }

    void
    SetAudioFrameCallback(const std::function<void(char *, int, double)> &cb) {
        audio_frame_cb_ = cb;
    }

    void SetAudioClockCallback(const std::function<double()> &cb) {
        audio_clock_cb_ = cb;
    }

    void SetPlayDoneCallback(const std::function<void(void)> &cb) {
        play_done_cb_ = cb;
    }

protected:
    void PlayVideoFrame(AVFrame *frame, double clock);
    void PlayAudioFrame(char *data, int length, double clock);

    bool ResampleFormatValid() const;

    bool InitInputContext();
    bool InitInputCodec();

    bool InitDecodeContext();
    bool InitSwrContext();
    bool InitSwsContext();

    void Release();

    void ResetInputContext();
    void ResetDecodeContext();
    void ResetHWDeviceContext();
    void ResetSwsContext();
    void ResetSwrContext();

    bool HandleVideoFrame(AVPacket *pkt);
    bool HandleAudioFrame(AVPacket *pkt);

    static int InterruptCallback(void *context);

    void StartThreads();
    void StopThreads();
    void JoinThreads();

    void SetRunning(bool run);
    void ReadThreadFunc();
    void VideoThreadFunc();
    void AudioThreadFunc();

protected:
    MediaSource media_source_;
    AudioFormat resample_fmt_;

    bool is_open_ = false;

    AvFunctionInterrupt interrupt_;

    // 视频硬件加速设备
    AVHWDeviceType hwtype_;

    // 输入
    AVFormatContext *fmt_ctx_ = nullptr;

    // video
    std::thread video_thread_;
    std::unique_ptr<VideoPlayer> video_player_;
    std::list<AVPacket *> video_queue_;
    std::mutex video_mutex_;
    std::condition_variable video_cv_;

    // audio
    std::thread audio_thread_;
    std::unique_ptr<AudioPlayer> audio_player_;
    std::list<AVPacket *> audio_queue_;
    std::mutex audio_mutex_;
    std::condition_variable audio_cv_;

    std::atomic_bool paused_;
    std::mutex paused_mutex_;
    std::condition_variable paused_cv_;

    std::atomic_bool running_;

    std::thread read_thread_;
    std::function<void(AVFrame *)> video_frame_cb_;
    std::function<void(char *, int, double)> audio_frame_cb_;
    std::function<double()> audio_clock_cb_;
    std::function<void(void)> play_done_cb_;
    std::shared_ptr<spdlog::logger> logger_;
};

class StreamPlayer {
public:
    StreamPlayer(int index, AVStream *);
    virtual ~StreamPlayer();

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

class VideoPlayer : public StreamPlayer {
public:
    VideoPlayer(int index, AVStream *stream);
    ~VideoPlayer();

    bool GetVideoFormat(VideoFormat *out_fmt);

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

class AudioPlayer : public StreamPlayer {
public:
    AudioPlayer(int index, AVStream *stream);
    ~AudioPlayer();

    bool GetSampleFormat(AudioFormat *out_fmt);

    void set_resample_format(AudioFormat fmt);

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
    AudioFormat resample_fmt_;
    SwrContext *swr_ctx_ = nullptr;

    std::function<void(char *, int, double)> frame_cb_;
};

const AVCodecHWConfig *AvUtilGetHwConfig(const AVCodec *codec,
                                         AVHWDeviceType hwtype);

#endif // FFPLAYER_H
