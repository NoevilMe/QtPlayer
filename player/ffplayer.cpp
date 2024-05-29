#include "ffplayer.h"
#include "av_util.h"

#include <fstream>

extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavdevice/avdevice.h>
// #include <libavformat/avformat.h>
// #include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
// #include <libavutil/timestamp.h>
};

#define OPEN_INPUT_TIMEOUT_MS 10000

std::shared_ptr<spdlog::logger> g_av_logger_;

const AVCodecHWConfig *AvUtilGetHwConfig(const AVCodec *codec,
                                         AVHWDeviceType hwtype) {
    const AVCodecHWConfig *hwconfig = nullptr;

    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config) {
            break;
        }

        if (g_av_logger_) {

            if (AV_HWDEVICE_TYPE_NONE == config->device_type) {
                // d3d11va_vld 没有加速器
                g_av_logger_->debug(
                    "found available hw config [None, {}] for codec {}",
                    avutil::GetPixFmtName(config->pix_fmt), codec->name);
            } else {
                g_av_logger_->debug(
                    "found available hw config [{}, {}] for codec {}",
                    avutil::GetHWDeviceTypeName(config->device_type),
                    avutil::GetPixFmtName(config->pix_fmt), codec->name);
            }
        }

        /**
         * The codec supports this format via the hw_device_ctx interface.
         *
         * When selecting this format, AVCodecContext.hw_device_ctx should
         * have been set to a device of the specified type before calling
         * avcodec_open2().
         */
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hwtype) {
            hwconfig = config;
        }
    }

    return hwconfig;
}

FFPlayer::FFPlayer() : hwtype_(avutil::GetDefaultHWDeviceType()) {
    logger_ = util::log::GetLogger(__func__);
    g_av_logger_ = logger_;

    audio_frame_ = av_frame_alloc();

    avutil::GetAllDevices();
}

FFPlayer::~FFPlayer() {
    Stop();

    ResetInputContext();
    ResetDecodeContext();

    if (audio_frame_) {
        av_frame_free(&audio_frame_);
        audio_frame_ = nullptr;
    }

    logger_->debug("~FFPlayer destroyed");
}

bool FFPlayer::Start() {
    if (media_source_.type == MediaType::kMediaNone) {
        logger_->error("no media source");
        return false;
    }

    if (!InitInputContext()) {
        return false;
    }

    if (!InitInputCodec()) {
        return false;
    }

    if (!InitDecodeContext()) {
        return false;
    }

    if (!InitSwsContext()) {
        return false;
    }

    if (!InitSwrContext()) {
        return false;
    }

    thd_ = std::thread([=]() { this->ThreadFunc(); });

    // std::vector<std::string> encoder_names{
    //     "h264_vaapi", "h264_qsv",    "h264_cuvid", "hevc_vaapi",  "hevc_qsv",
    //     "hevc_cuvid", "mjpeg_vaapi", "mjpeg_qsv",  "mjpeg_cuvid",
    // };

    // for (auto &name : encoder_names) {
    //     const AVCodec *encoder = avcodec_find_encoder_by_name(name.data());
    //     if (!encoder) {
    //         logger_->warn("no encoder {}", name);
    //     } else {
    //         logger_->info("encoder {} yes, {}", encoder->name,
    //                       encoder->long_name);
    //     }
    // }

    // for (auto &name : encoder_names) {
    //     const AVCodec *decoder = avcodec_find_decoder_by_name(name.data());
    //     if (!decoder) {
    //         logger_->warn("no decoder {}", name);
    //     } else {
    //         logger_->info("decoder {} yes, {}", decoder->name,
    //                       decoder->long_name);
    //     }
    // }

    return true;
}

void FFPlayer::Stop() {
    running_.store(false);
    if (thd_.joinable()) {
        thd_.join();
    }

    if (video_player_) {
        video_player_.reset();
    }

    media_source_.type = MediaType::kMediaNone;
    media_source_.src.clear();
}

void FFPlayer::SetMediaSource(MediaSource media) {
    media_source_ = std::move(media);
}

void FFPlayer::SetAudioDeviceFormat(AudioDeviceFormat fmt) {
    resample_fmt_.sample_rate = fmt.sample_rate;
    resample_fmt_.channel_count = fmt.channel_count;

    switch (fmt.sample_fmt) {
    case AudioSampleFormat::UInt8:
        resample_fmt_.sample_fmt = AV_SAMPLE_FMT_U8;
        break;
    case AudioSampleFormat::Int16:
        resample_fmt_.sample_fmt = AV_SAMPLE_FMT_S16;
        break;
    case AudioSampleFormat::Int32:
        resample_fmt_.sample_fmt = AV_SAMPLE_FMT_S32;
        break;
    case AudioSampleFormat::Float:
        resample_fmt_.sample_fmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        //输出的采样格式。绝⼤部分声卡⽀持
        resample_fmt_.sample_fmt = AV_SAMPLE_FMT_S16;
        break;
    }
}

void FFPlayer::PlayVideoFrame(AVFrame *frame, double clock) {
    auto audio_clock = audio_clock_cb_();
    auto diff = clock - audio_clock;
    if (diff > 0.04) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((long long)(diff * 1000)));

        audio_clock = audio_clock_cb_();

        logger_->info("video frame pts {}, clock {}, audio clock {}, diff {}",
                      frame->pts, clock, audio_clock, clock - audio_clock);
    } else {
        logger_->info("video frame pts {}, clock {}, audio clock {}, diff {}",
                      frame->pts, clock, audio_clock, clock - audio_clock);
    }

    video_frame_cb_(frame);
}

void FFPlayer::PlayAudioFrame(char *data, int length, double clock) {
    if (audio_frame_cb_) {
        audio_frame_cb_(data, length, clock);
        logger_->debug("audio_frame_cb_ {}, {}, {}", (void *)data, length,
                       clock);
    }
}

bool FFPlayer::ResampleFormatValid() const {
    return resample_fmt_.channel_count > 0 && resample_fmt_.sample_rate > 0 &&
           resample_fmt_.sample_fmt != AV_SAMPLE_FMT_NONE;
}

// https://www.cnblogs.com/feiyangqingyun/p/16875945.html
//  ffplay -f dshow -i video="USB Video Device" -s 1280x720 -framerate 30
bool FFPlayer::InitInputContext() {
    std::string url;

    const AVInputFormat *input_fmt = nullptr;

    switch (media_source_.type) {
    case MediaType::kMediaCapture: {
        url = "video=";
        url += media_source_.src;
#ifdef _WIN32
        const char drive[] = "dshow";
#elif defined(__linux__)
        const char drive[] = "v4l2";
#else
        const char drive[] = "avfoundation";
#endif
        input_fmt = av_find_input_format(drive);
        if (input_fmt == nullptr) {
            logger_->error("can not find {}", drive);
            return false;
        }
    } break;
    case MediaType::kMediaFile:
    case MediaType::kMediaNetwork:
        url = media_source_.src;
        break;
    default:
        logger_->error("不支持的媒体类型{}", (int)media_source_.type);
        return false;
    }

    fmt_ctx_ = avformat_alloc_context();

    if (!fmt_ctx_) {
        logger_->error("avformat_alloc_context fail");
        return false;
    }

    fmt_ctx_->flags |= AVFMT_FLAG_NONBLOCK; // 拔掉摄像头不阻塞

    // set input options
    AVDictionary *options = nullptr;

    if (media_source_.type == MediaType::kMediaCapture) {
        // av_dict_set(&options, "fflags", "nobuffer", 0);
        // av_dict_set(&options, "max_delay", "100000", 0);
        // av_dict_set(&options, "framerate", "30", 0);
        // av_dict_set(&options, "probesize", "100000000", 0);
        // av_dict_set(&options, "analyzeduration", "5000000", 0);

        // framerate needs to set before opening the v4l2 device
        //   av_dict_set(&options, "framerate", "15", 0);
        // This will not work if the camera does not support h264. In that case
        // remove this line. I wrote this for Raspberry Pi where the camera
        // driver can stream h264. av_dict_set(&options, "input_format", "h264",
        // 0); av_dict_set(&options, "pixel_format", "yuvj420p", 0);

        // 如下几个有顺序，前面的会限制后面
        av_dict_set(&options, "video_size", "1920x1080", 0);
        av_dict_set(&options, "framerate", "25", 0);
        // av_dict_set(&options, "input_format", "mjpeg", 0);
        // av_dict_set(&options, "pixel_format", "nv12", 0);
        // av_dict_set(&options, "pixel_format", "yuyv422", 0);

        // av_dict_set(&options, "pixel_format", "rgb24", 0);

        // https://superuser.com/questions/1310236/tell-ffmpeg-to-drop-frames-to-reduce-memory-usage
        // av_dict_set_int(&options, "rtbufsize", 18432000, 0);
        // av_dict_set (& options, "stimeout", "10000000", 0);//Set timeout
        // disconnect time
    }
    // av_dict_set(&options, "buffer_size", "2048000", 0);

    util::AtExit ao([&]() { av_dict_free(&options); });

    fmt_ctx_->interrupt_callback.opaque = this;
    fmt_ctx_->interrupt_callback.callback = InterruptCallback;

    interrupt_.func_start_timestamp = util::TimeMilliseconds();

    auto err = avformat_open_input(&fmt_ctx_, url.data(), input_fmt, &options);
    if (err) {
        logger_->error("avformat_open_input device {}, {}, {}", url, err,
                       avutil::ErrorString(err));
        return false;
    }

    if (interrupt_.interrupted) {
        logger_->error("can't open input device {}, timeout", url);
        return false;
    }

    logger_->debug("avformat_open_input success");

    // 1.2 解码一段数据，获取流相关信息 https://zhuanlan.zhihu.com/p/639412354
    //    input_fmt_ctx_->probesize = 1000 * 1024;
    //    input_fmt_ctx_->max_analyze_duration = 5 * AV_TIME_BASE;

    interrupt_.func_start_timestamp = util::TimeMilliseconds();
    if (avformat_find_stream_info(fmt_ctx_, 0) < 0) {
        logger_->error("failed to retrieve input stream information");
        return false;
    }

    if (interrupt_.interrupted) {
        logger_->error("can't avformat_find_stream_info, timeout");
        return false;
    }

    logger_->debug("avformat_find_stream_info success");

    // 1.3 获取输入ctx
    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; ++i) {
        auto stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            !video_player_) {
            video_player_.reset(new VideoPlayer(i, stream));
            video_player_->SetFrameCallback(
                std::bind(&FFPlayer::PlayVideoFrame, this,
                          std::placeholders::_1, std::placeholders::_2));

        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                   !audio_player_) {
            audio_player_.reset(new AudioPlayer(i, stream));
            audio_player_->SetFrameCallback(std::bind(
                &FFPlayer::PlayAudioFrame, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
            audio_player_->set_resample_format(resample_fmt_);
        }
    }

    if (video_player_ < 0) {
        logger_->error("no video stream in input stream");
        return false;
    }

    // 输出调试信息：tbr代表帧率；tbn代表文件层（st）的时间精度，即1S=1200k，和duration相关；tbc代表视频层（st->codec）的时间精度，即1S=XX，和stream->duration和时间戳相关。
    std::string name(fmt::format("@ {}", url));
    av_dump_format(fmt_ctx_, 0, name.data(), 0);

    video_player_->LogInput();
    if (audio_player_) {
        audio_player_->LogInput();
    }

    return true;
}

bool FFPlayer::InitInputCodec() {
    // decoder, 数据宽度都有可能和图片宽度不一致，需要裁剪。
    // AV_HWDEVICE_TYPE_DXVA2 支持map，但是尺寸会发生变化, 空隙很多
    // AV_HWDEVICE_TYPE_D3D11VA 不支持map
    // AV_HWDEVICE_TYPE_QSV 不支持map
    //    hwtype_ = AV_HWDEVICE_TYPE_DXVA2; //AV_HWDEVICE_TYPE_D3D11VA;

    // hwtype_ = AV_HWDEVICE_TYPE_D3D11VA;

    if (video_player_) {
        std::vector<AVHWDeviceType> try_hwdevice_types;

        AVCodecID video_codec_id = video_player_->CodecID();
        if (video_codec_id == AV_CODEC_ID_MJPEG) {
            // windows mjpeg支持3种解码器mjpeg mjpeg_qsv mjpeg_cuvid。
            // DXVA2和D3D11VA不支持mjpeg
            if (hwtype_ == AV_HWDEVICE_TYPE_QSV) {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_QSV,
                                           AV_HWDEVICE_TYPE_CUDA,
                                           AV_HWDEVICE_TYPE_NONE});
            } else if (hwtype_ == AV_HWDEVICE_TYPE_CUDA) {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_CUDA,
                                           AV_HWDEVICE_TYPE_QSV,
                                           AV_HWDEVICE_TYPE_NONE});
            } else {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_NONE});
            }
        } else {
            if (hwtype_ == AV_HWDEVICE_TYPE_QSV) {
                try_hwdevice_types.assign(
                    {AV_HWDEVICE_TYPE_QSV, AV_HWDEVICE_TYPE_CUDA,
                     AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2,
                     AV_HWDEVICE_TYPE_NONE});
            } else if (hwtype_ == AV_HWDEVICE_TYPE_CUDA) {
                try_hwdevice_types.assign(
                    {AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_QSV,
                     AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2,
                     AV_HWDEVICE_TYPE_NONE});
            } else if (hwtype_ == AV_HWDEVICE_TYPE_D3D11VA) {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_D3D11VA,
                                           AV_HWDEVICE_TYPE_DXVA2,
                                           AV_HWDEVICE_TYPE_NONE});
            } else if (hwtype_ == AV_HWDEVICE_TYPE_DXVA2) {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_DXVA2,
                                           AV_HWDEVICE_TYPE_D3D11VA,
                                           AV_HWDEVICE_TYPE_NONE});
            } else {
                try_hwdevice_types.assign({AV_HWDEVICE_TYPE_NONE});
            }
        }

        std::string codec_name = avutil::GetCodecName(video_codec_id);
        logger_->debug("codec name {}", codec_name);

        for (auto hwtype : try_hwdevice_types) {
            std::string decoder_name =
                codec_name + avutil::GetDecoderSuffixByHWDeviceType(hwtype);

            const AVCodec *codec =
                avcodec_find_decoder_by_name(decoder_name.data());
            if (!codec) {
                logger_->warn("can not find decoder by name {}", decoder_name);
                continue;
            }

            logger_->info("try input video codec {}:{}", codec->name,
                          codec->long_name);

            if (hwtype == AV_HWDEVICE_TYPE_NONE ||
                video_player_->InitHWDeviceContext(codec, hwtype)) {
                video_player_->set_codec(codec); //非硬件加速的时候必须明确设置
                video_player_->LogHw();
                break;
            }
        }

        if (!video_player_->codec()) {
            logger_->error("video player codec is not set for {}", codec_name);
            return false;
        }
    }

    if (audio_player_) {
        const AVCodec *audio_codec =
            avcodec_find_decoder(audio_player_->CodecID());
        if (audio_codec) {
            logger_->info("input audio codec {}:{}", audio_codec->name,
                          audio_codec->long_name);
            audio_player_->set_codec(audio_codec);
        } else {
            logger_->error("failed to find audio decoder of codec id {}",
                           avutil::GetCodecName(audio_player_->CodecID()));
            return false;
        }
    }

    return true;
}

bool FFPlayer::InitDecodeContext() {

    if (!video_player_->InitDecodeContext()) {
        return false;
    }

    if (audio_player_ && !audio_player_->InitDecodeContext()) {
        return false;
    }

    logger_->debug("InitInputDecodeContext success");
    return true;
}

bool FFPlayer::InitSwrContext() {
    if (!audio_player_)
        return true;

    return audio_player_->InitSwrContext();
}

bool FFPlayer::InitSwsContext() { return video_player_->InitSwsContext(); }

void FFPlayer::ResetInputContext() {
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
}

void FFPlayer::ResetDecodeContext() {
    if (video_player_)
        video_player_->ResetDecodeContext();

    if (audio_player_)
        audio_player_->ResetDecodeContext();
}

void FFPlayer::ResetSwrContext() {
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }
}

bool FFPlayer::HandleVideoFrame(AVPacket *pkt) {
    return video_player_->HandleFrame(pkt);
}

bool FFPlayer::HandleAudioFrame(AVPacket *pkt) {
    return audio_player_->HandleFrame(pkt);
}

int FFPlayer::InterruptCallback(void *context) {
    FFPlayer *obj = (FFPlayer *)context;
    obj->interrupt_.func_end_timestamp = util::TimeMilliseconds();
    if (obj->interrupt_.func_end_timestamp -
            obj->interrupt_.func_start_timestamp >
        OPEN_INPUT_TIMEOUT_MS) {
        obj->logger_->error("device timeout {} ms", OPEN_INPUT_TIMEOUT_MS);
        obj->interrupt_.interrupted = true;
        return 1;
    } else {
        return 0;
    }
}

void FFPlayer::ThreadFunc() {
    running_.store(true);
    logger_->info("running ...");

    util::AtExit er([=]() {
        running_.store(false);
        logger_->info("running done");
    });

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        logger_->error("av_packet_alloc error");
        return;
    }

    util::AtExit ep([&]() { av_packet_free(&pkt); });

    try {
        int err = 0;
        while (running_.load()) {
            interrupt_.func_start_timestamp = util::TimeMilliseconds();
            err = av_read_frame(fmt_ctx_, pkt);
            if (0 == err) {
                // pkt->time_base = {1, 10000000};
                logger_->trace("-----------------------------");
                logger_->trace("av_read_frame ok, index {}, size {}, timebase "
                               "{}/{}, pts {}, duration {}",
                               pkt->stream_index, pkt->size, pkt->time_base.num,
                               pkt->time_base.den, pkt->pts, pkt->duration);

                if (video_player_ &&
                    video_player_->index() == pkt->stream_index) {
                    this->HandleVideoFrame(pkt);
                } else if (audio_player_ &&
                           audio_player_->index() == pkt->stream_index) {
                    this->HandleAudioFrame(pkt);
                }

                av_packet_unref(pkt);
            } else if (AVERROR(EAGAIN) == err) {
                logger_->trace("av_read_frame AVERROR(EAGAIN)");
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            } else {
                logger_->error("ffmpeg av_read_frame failure, {}",
                               avutil::ErrorString(err));
                break;
            }
        }
    } catch (std::runtime_error &e) {
        logger_->error("runtime_error, {}", e.what());
    } catch (std::exception &e) {
        logger_->error("exception, {}", e.what());
    } catch (...) {
        logger_->error("run unknown exception");
    }

    logger_->info("run end");
}

AVPlayer::AVPlayer(int idx, AVStream *strm) : index_(idx), stream_(strm) {
    if (!stream_)
        throw std::runtime_error("AVStream is null");

    decoded_frame_ = av_frame_alloc();
    if (!decoded_frame_)
        throw std::runtime_error("av_frame_alloc fail");
}

AVPlayer::~AVPlayer() {
    ResetDecodeContext();
    if (decoded_frame_) {
        av_frame_free(&decoded_frame_);
        decoded_frame_ = nullptr;
    }
}

void AVPlayer::ResetDecodeContext() {
    if (decode_ctx_) {
        if (decode_ctx_->hw_device_ctx) {
            av_buffer_unref(&decode_ctx_->hw_device_ctx);
        }

        avcodec_close(decode_ctx_);
        avcodec_free_context(&decode_ctx_);
        decode_ctx_ = nullptr;
    }
}

AVCodecID AVPlayer::CodecID() const { return stream_->codecpar->codec_id; }

AVCodecParameters *AVPlayer::CodecPar() const { return stream_->codecpar; }

AVRational AVPlayer::TimeBase() { return stream_->time_base; }

void AVPlayer::set_codec(const AVCodec *c) { codec_ = c; }

VideoPlayer::VideoPlayer(int index, AVStream *stream)
    : AVPlayer(index, stream) {
    logger_ = util::log::GetLogger(__func__);
}

VideoPlayer::~VideoPlayer() {
    ResetSwsContext();
    ResetHWDeviceContext();
}

int VideoPlayer::Fps() { return (int)av_q2d(stream_->avg_frame_rate); }

bool VideoPlayer::InitHWDeviceContext(const AVCodec *codec,
                                      AVHWDeviceType hwtype) {
    auto hwconfig = AvUtilGetHwConfig(codec, hwtype);
    if (!hwconfig) {
        logger_->error("can not get hwaccel {} config for {}",
                       avutil::GetHWDeviceTypeName(hwtype), codec->long_name);
        return false;
    }

    // 如果以前创建过，则先释放
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }

    int err = av_hwdevice_ctx_create(&hw_device_ctx_, hwconfig->device_type,
                                     nullptr, nullptr, 0);
    if (err) {
        logger_->error("failed to av_hwdevice_ctx_create, {}",
                       avutil::ErrorString(err));
        return false;
    }

    // codec_ = codec; 公共函数设置
    hw_pix_fmt_ = hwconfig->pix_fmt;
    hwtype_ = hwtype;

    logger_->info("apply hw config [{}, {}] for codec {}",
                  avutil::GetHWDeviceTypeName(hwconfig->device_type),
                  avutil::GetPixFmtName(hwconfig->pix_fmt), codec->name);

    logger_->debug("InitHWDeviceContext success");

    return true;
}

AVPixelFormat VideoPlayer::GetHwFormat(AVCodecContext *ctx,
                                       const AVPixelFormat *pix_fmts) {
    VideoPlayer *inst = static_cast<VideoPlayer *>(ctx->opaque);
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == inst->hw_pix_fmt_) {
            return *p;
        }
    }

    inst->logger_->error("Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

void VideoPlayer::LogInput() {
    auto video_codecpar = stream_->codecpar;
    logger_->info("input streams video index = {}, codec id {}, pix fmt {}, "
                  "avg fps is {}, resolution {}x{}, time_base {}/{}",
                  index_, avutil::GetCodecName(video_codecpar->codec_id),
                  avutil::GetPixFmtName((AVPixelFormat)video_codecpar->format),
                  (int)av_q2d(stream_->avg_frame_rate), video_codecpar->width,
                  video_codecpar->height, stream_->time_base.num,
                  stream_->time_base.den);
}

void VideoPlayer::LogHw() {
    if (!codec_) {
        logger_->error("LogHw fail, codec is null");
    } else {
        logger_->info("select hw device {}, codec {}:{}",
                      avutil::GetHWDeviceTypeName(hwtype_), codec_->name,
                      codec_->long_name);
    }
}

bool VideoPlayer::InitDecodeContext() {
    if (!codec_) {
        logger_->error("InitDecodeContext fail, AVCodec is null");
        return false;
    }

    decode_ctx_ = avcodec_alloc_context3(codec_);
    avcodec_parameters_to_context(decode_ctx_, stream_->codecpar);

    logger_->debug("video input stream time_base {}, {}",
                   stream_->time_base.num, stream_->time_base.den);
    logger_->debug("video input stream avg_frame_rate {}, {}",
                   stream_->avg_frame_rate.num, stream_->avg_frame_rate.den);

    // decoder time_base is not equal to stream
    logger_->info("video input decoder time_base {}, {}",
                  decode_ctx_->time_base.num, decode_ctx_->time_base.den);
    logger_->info("video input decoder framerate {}, {}",
                  decode_ctx_->framerate.num, decode_ctx_->framerate.den);

    if (hw_device_ctx_) {
        decode_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
        decode_ctx_->opaque = this;
        decode_ctx_->get_format = GetHwFormat;
    }

    AVDictionary *codec_opts = nullptr;

    util::AtExit e([&]() {
        if (codec_opts) {
            av_dict_free(&codec_opts);
        }
    });

    av_dict_set(&codec_opts, "refcounted_frames", "1", 0);
    //    decode_ctx_->thread_count = 8;

    int err = avcodec_open2(decode_ctx_, codec_, NULL);
    if (err < 0) {
        logger_->error("failed to avcodec_open2, {}", avutil::ErrorString(err));
        return false;
    }

    stream_->discard = AVDISCARD_DEFAULT;
    logger_->info("video decode output fmt {}, resolution {}x{}",
                  avutil::GetPixFmtName(decode_ctx_->pix_fmt),
                  decode_ctx_->width, decode_ctx_->height);

    return true;
}

bool VideoPlayer::InitSwsContext() {
    if (hwtype_ == AV_HWDEVICE_TYPE_NONE && decode_ctx_->pix_fmt != sws_fmt_) {
        // AV_PIX_FMT_YUV420P
        sws_width_ = decode_ctx_->width >> 2 << 2; // align = 4
        sws_height_ = decode_ctx_->height;

        logger_->debug("sws_scale dest {}x{}, pix_fmt {}", sws_width_,
                       sws_height_, avutil::GetPixFmtName(sws_fmt_));

        sws_ctx_ = sws_getContext(decode_ctx_->width, decode_ctx_->height,
                                  decode_ctx_->pix_fmt, sws_width_, sws_height_,
                                  sws_fmt_, SWS_BICUBIC, NULL, NULL, NULL);
        if (!sws_ctx_) {
            logger_->error("sws_getContext fail");
            return false;
        }

        return true;
    }

    return true;
}

void VideoPlayer::ResetHWDeviceContext() {
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }
}

void VideoPlayer::ResetSwsContext() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
}

bool VideoPlayer::HandleFrame(AVPacket *pkt) {
    if (!pkt || pkt->size <= 0)
        return false;

    ts_start_ = util::TimeMilliseconds();

    int ret = avcodec_send_packet(decode_ctx_, pkt);
    if (AVERROR(EAGAIN) == ret) {
        logger_->error("send packet failure, AVERROR(EAGAIN), input is not "
                       "accepted in the current state");
        return false;
    } else if (AVERROR_EOF == ret) {
        logger_->error("send packet failure, AVERROR_EOF, the decoder has been "
                       "flushed, and no new packets can be sent to it (also "
                       "returned if more than 1 flush packet is sent");
        return false;
    } else if (AVERROR(EINVAL) == ret) {
        logger_->error("send packet failure, AVERROR(EINVAL), codec not "
                       "opened, it is an encoder, or requires flush");
        return false;
    } else if (AVERROR(ENOMEM) == ret) {
        logger_->error("send packet failure, AVERROR(ENOMEM), failed to add "
                       "packet to internal queue, or similar other errors: "
                       "legitimate decoding errors");
        return false;
    } else if (ret < 0) {
        logger_->error("send packet failure, {}", avutil::ErrorString(ret));
        return false;
    }
    logger_->trace("avcodec_send_packet ok");

    while (ret >= 0) {
        ret = avcodec_receive_frame(decode_ctx_, decoded_frame_);

        if (pkt && ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            logger_->error("avcodec_receive_frame frame failure, {}",
                           avutil::ErrorString(ret));
            return false;
        }

        ts_decode_ = util::TimeMilliseconds();
        logger_->trace(
            "avcodec_receive_frame ok, fmt {}, resolution {}x{}, pts {}, cost "
            "{}, linesize {}",
            avutil::GetPixFmtName((AVPixelFormat)decoded_frame_->format),
            decoded_frame_->width, decoded_frame_->height, decoded_frame_->pts,
            ts_decode_ - ts_start_, decoded_frame_->linesize[0]);

        util::AtExit r([&]() { av_frame_unref(decoded_frame_); });

        AVFrame *data_frame = nullptr;
        if (hw_pix_fmt_ == decoded_frame_->format) {
            logger_->debug(
                "hw frame {}, color_primaries {}, w {}, h {}, "
                "yw {}, uw {}, vw {}",
                avutil::GetPixFmtName((AVPixelFormat)decoded_frame_->format),
                avutil::GetColorPrimariesName(decoded_frame_->color_primaries),
                decoded_frame_->width, decoded_frame_->height,
                decoded_frame_->linesize[0], decoded_frame_->linesize[1],
                decoded_frame_->linesize[2]);

            // 如果采用的硬件加速剂，则调用avcodec_receive_frame()函数后，解码后的数据还在GPU中，所以需要通过此函数
            // 将GPU中的数据转移到CPU中来
            data_frame = av_frame_alloc();

            if (map_hw_frame_) {
                int ret = av_hwframe_map(data_frame, decoded_frame_,
                                         AV_HWFRAME_MAP_READ); // 映射硬件数据帧
                if (ret < 0) {
                    logger_->error(
                        "av_hwframe_map fail, {}, disable hw frame mapping",
                        avutil::ErrorString(ret));
                    map_hw_frame_ = false;
                    return false;
                }

                ts_hw_ = util::TimeMilliseconds();
                logger_->debug("map cost {}", ts_hw_ - ts_decode_);

                // map之后的宽高是没设置的
                data_frame->width = decoded_frame_->width;
                data_frame->height = decoded_frame_->height;

                logger_->debug(
                    "mapped frame {}, color_primaries {}, w {}, h {}, "
                    "yw {}, uw {}, vw {}",
                    av_get_pix_fmt_name((AVPixelFormat)data_frame->format),
                    data_frame->color_primaries, data_frame->width,
                    data_frame->height, data_frame->linesize[0],
                    data_frame->linesize[1], data_frame->linesize[2]);
            } else {
                if ((ret = av_hwframe_transfer_data(data_frame, decoded_frame_,
                                                    0)) < 0) {
                    logger_->error("av_hwframe_transfer_data fail, {}",
                                   avutil::ErrorString(ret));
                    return false;
                }

                ts_hw_ = util::TimeMilliseconds();
                logger_->debug("transfer cost {}", ts_hw_ - ts_decode_);

                // pts需要设置
                data_frame->pts = decoded_frame_->pts;

                logger_->debug(
                    "transfer frame {}, color_primaries {}, w {}, h {}, "
                    "yw {}, uw {}, vw {}, pts {}",
                    av_get_pix_fmt_name((AVPixelFormat)data_frame->format),
                    avutil::GetColorPrimariesName(data_frame->color_primaries),
                    data_frame->width, data_frame->height,
                    data_frame->linesize[0], data_frame->linesize[1],
                    data_frame->linesize[2], data_frame->pts);
            }

        } else {
            if (sws_ctx_) {
                data_frame = av_frame_alloc();
                if (!data_frame) {
                    logger_->error("av_frame_alloc fail, OOM");
                    return false;
                }
                data_frame->format = sws_fmt_;
                data_frame->width = sws_width_;
                data_frame->height = sws_height_;
                int err = av_frame_get_buffer(data_frame, 0);
                if (err < 0) {
                    logger_->error("av_frame_get_buffer fail, {}",
                                   avutil::ErrorString((err)));
                    av_frame_free(&data_frame);
                    return false;
                }

                // if (av_frame_make_writable(scale_frame) < 0) {
                //     FATAL("scale frame is not writable");
                // }

                int h = sws_scale(sws_ctx_, decoded_frame_->data,
                                  decoded_frame_->linesize, 0,
                                  decoded_frame_->height, data_frame->data,
                                  data_frame->linesize);
                if (h <= 0 || h != data_frame->height) {
                    logger_->error("sws_scale height error {}", h);
                    return false;
                }

                ts_sws_ = util::TimeMilliseconds();
                logger_->trace("sws_scale cost {}", ts_sws_ - ts_decode_);

                logger_->trace(
                    "sws_scale ok, fmt {}, resolution {}x{}",
                    avutil::GetPixFmtName((AVPixelFormat)data_frame->format),
                    data_frame->width, data_frame->height);

            } else {
                data_frame = decoded_frame_;
            }
        }

        if (frame_cb_) {
            ts_cb_ = util::TimeMilliseconds();
            logger_->debug("handle frame cost {}", ts_cb_ - ts_start_);

            auto video_clock = data_frame->pts * av_q2d(stream_->time_base);

            frame_cb_(data_frame, video_clock);
        }

        if (data_frame != decoded_frame_) {
            av_frame_free(&data_frame);
        }
    }

    return true;
}

AudioPlayer::AudioPlayer(int index, AVStream *stream)
    : AVPlayer(index, stream) {
    logger_ = util::log::GetLogger(__func__);
}

AudioPlayer::~AudioPlayer() {}

void AudioPlayer::set_resample_format(ResampleFormat fmt) {
    resample_fmt_ = fmt;
}

void AudioPlayer::LogInput() {

    auto audio_codecpar = stream_->codecpar;
    logger_->info(
        "input streams audio index = {}, codec id {}, sample fmt {}, sample "
        "rate {}, channels {}, bits per sample {}, time_base {}/{}",
        index_, avutil::GetCodecName(audio_codecpar->codec_id),
        avutil::GetSampleFmtName((AVSampleFormat)audio_codecpar->format),
        audio_codecpar->sample_rate, audio_codecpar->ch_layout.nb_channels,
        audio_codecpar->bits_per_coded_sample, stream_->time_base.num,
        stream_->time_base.den);
}

bool AudioPlayer::InitDecodeContext() {
    if (!codec_) {
        logger_->error("InitDecodeContext fail, AVCodec is null");
        return false;
    }

    decode_ctx_ = avcodec_alloc_context3(codec_);
    avcodec_parameters_to_context(decode_ctx_, stream_->codecpar);

    logger_->debug("audio input stream time_base {}, {}",
                   stream_->time_base.num, stream_->time_base.den);

    auto err = avcodec_open2(decode_ctx_, codec_, NULL);
    if (err < 0) {
        logger_->error("failed to avcodec_open2, {}", avutil::ErrorString(err));

        avcodec_free_context(&decode_ctx_);
        decode_ctx_ = nullptr;
    } else {
        logger_->info("audio decoder time base {}/{}, output fmt {}, sample "
                      "rate {}, channels {}, channel layout {}",
                      decode_ctx_->time_base.num, decode_ctx_->time_base.den,
                      avutil::GetSampleFmtName(decode_ctx_->sample_fmt),
                      decode_ctx_->sample_rate,
                      decode_ctx_->ch_layout.nb_channels,
                      avutil::ChannelLayoutDescribe(&decode_ctx_->ch_layout));
    }
}

bool AudioPlayer::HandleFrame(AVPacket *pkt) {
    if (!pkt || pkt->size <= 0)
        return false;

    int ret = avcodec_send_packet(decode_ctx_, pkt);
    if (AVERROR(EAGAIN) == ret) {
        logger_->error(
            "send audio packet failure, AVERROR(EAGAIN), input is not "
            "accepted in the current state");
        return false;
    } else if (AVERROR_EOF == ret) {
        logger_->error(
            "send audio packet failure, AVERROR_EOF, the decoder has been "
            "flushed, and no new packets can be sent to it (also "
            "returned if more than 1 flush packet is sent");
        return false;
    } else if (AVERROR(EINVAL) == ret) {
        logger_->error("send audio packet failure, AVERROR(EINVAL), codec not "
                       "opened, it is an encoder, or requires flush");
        return false;
    } else if (AVERROR(ENOMEM) == ret) {
        logger_->error(
            "send audio packet failure, AVERROR(ENOMEM), failed to add "
            "packet to internal queue, or similar other errors: "
            "legitimate decoding errors");
        return false;
    } else if (ret < 0) {
        logger_->error("send packet failure, {}", avutil::ErrorString(ret));
        return false;
    }
    logger_->trace("audio avcodec_send_packet ok");

    while (ret >= 0) {
        ret = avcodec_receive_frame(decode_ctx_, decoded_frame_);

        if (pkt && ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            logger_->error("avcodec_receive_frame audio frame failure, {}",
                           avutil::ErrorString(ret));
            return false;
        }

        util::AtExit r([&]() { av_frame_unref(decoded_frame_); });

        logger_->trace(
            "audio avcodec_receive_frame ok, sample fmt {}, sample rate {}, "
            "channels {}, nb_samples {}, duration {}, pts {}",
            avutil::GetSampleFmtName((AVSampleFormat)decoded_frame_->format),
            decoded_frame_->sample_rate, decoded_frame_->ch_layout.nb_channels,
            decoded_frame_->nb_samples, decoded_frame_->duration,
            decoded_frame_->pts);

        // https://www.cnblogs.com/zjacky/p/16529648.html 可以写入AVFrame

        // https://blog.csdn.net/u012117034/article/details/127537875
        //        auto out_count = (int64_t)audio_frame_->nb_samples *
        //                             resample_fmt_.sample_rate /
        //                             audio_frame_->sample_rate +
        //                         256;

        if (swr_ctx_) {
            // 采样数空间留有余量
            auto out_count = decoded_frame_->nb_samples * 3 / 2;

            auto audio_buf_len = av_samples_get_buffer_size(
                nullptr, resample_fmt_.channel_count, out_count,
                resample_fmt_.sample_fmt, 0);
            logger_->trace(
                "out count {}, av_samples_get_buffer_size out size {} ",
                out_count, audio_buf_len);

            uint8_t *audio_buf = new uint8_t[audio_buf_len];

            // av_samples_alloc_array_and_samples
            // av_samples_alloc和av_samples_get_buffer_size的计算空间是一样的。
            //        int audio_buf_len =
            //            av_samples_alloc(&audio_buf, nullptr,
            //            resample_fmt_.channel_count,
            //                             out_count, resample_fmt_.sample_fmt,
            //                             0);
            //        logger_->trace("av_samples_alloc nb_samples {},
            //        audio_buf_len
            //        {}",
            //                       out_count, audio_buf_len);

            // 对于音频来说，extended_data 和
            // data是一样的。音频更常用extended_data来表示
            const uint8_t **in_data =
                (const uint8_t **)decoded_frame_->extended_data;
            int in_count = decoded_frame_->nb_samples;

            // swr_convert_frame。输出每帧的采样数
            int out_nb_samples = swr_convert(swr_ctx_, &audio_buf,
                                             audio_buf_len, in_data, in_count);
            logger_->trace("out_nb_samples {}", out_nb_samples);
            if (out_nb_samples < 0) {
                logger_->error("swr_convert error, {}",
                               avutil::ErrorString(out_nb_samples));
                return false;
            }

            int data_size = out_nb_samples * resample_fmt_.channel_count *
                            av_get_bytes_per_sample(resample_fmt_.sample_fmt);
            logger_->trace("resample frame data size {}", data_size);

            double clock = decoded_frame_->pts * av_q2d(stream_->time_base);
            logger_->debug("audio frame pts {},  clock {}", decoded_frame_->pts,
                           clock);
            if (frame_cb_) {
                frame_cb_((char *)audio_buf, data_size, clock);
                logger_->debug("frame_cb_ {}, {}, {}", (void *)audio_buf,
                               data_size, clock);
            } else {
                delete[] audio_buf;
            }

            // FIXME:
            std::this_thread::sleep_for(std::chrono::microseconds(23220));

            /*
        // 转码音频帧
        // 计算转码后的音频数据大小
        int dstNbSamples = av_rescale_rnd(swr_get_delay(swrCtx, 44100) +
        aacFrame->nb_samples, 44100, 44100, AV_ROUND_UP); int
        dstBufferSize = av_samples_get_buffer_size(nullptr, 2,
        dstNbSamples, AV_SAMPLE_FMT_S16, 0);

        // 分配转码后的音频数据缓冲区
        uint8_t *dstBuffer = static_cast<uint8_t
        *>(av_malloc(dstBufferSize));

        // 进行音频转码
        int numSamples = swr_convert(audioSwsContext, &dstBuffer,
        dstNbSamples, const_cast<const uint8_t **>(pAudioFrame->data),
        pAudioFrame->nb_samples); if (numSamples < 0) { qDebug() <<
        "音频转码失败"; av_freep(&dstBuffer);
        }
        else{
            // 释放资源
            // 将音频帧数据写入音频输出设备
            outputDevice->write((const char *)dstBuffer, dstBufferSize);
        }

        // 计算音频帧播放时长
        AVRational timeBase =
        pFormatContext->streams[audioStream]->time_base; int64_t pts =
        av_frame_get_best_effort_timestamp(pAudioFrame); double time =
        av_q2d(timeBase) * pts;

        // 延时播放下一帧
        QEventLoop loop;
        QTimer::singleShot(time * 1000, &loop, [&]() { loop.quit(); });
        loop.exec(); */

            // uint8_t *data[2] = {0};
            //                if(!pcm)pcm = new
            //                uint8_t[frame->nb_samples*2*2]; data[0] = pcm;
            ////                int swr_convert(struct SwrContext *s,
            /// uint8_t **out,
            /// int out_count, / const uint8_t **in , int in_count);

            //               ret = swr_convert(actx,
            //                                   data, frame->nb_samples,
            //                                   //输出 (const
            //                                   uint8_t**)frame->data,frame->nb_samples
            //                                   //输入
        }
    }

    return true;
}

bool AudioPlayer::InitSwrContext() {
    if (!decode_ctx_)
        return true;

    if (!ResampleFormatValid()) {
        logger_->warn("resample format is not valid");
        return true;
    }

    // 创建 SwrContext 对象
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的layout,
    av_channel_layout_default(&out_ch_layout, resample_fmt_.channel_count);

    AVChannelLayout in_ch_layout;
    av_channel_layout_copy(&in_ch_layout, &decode_ctx_->ch_layout);

    int ret = swr_alloc_set_opts2(&swr_ctx_, &out_ch_layout,
                                  resample_fmt_.sample_fmt, //输出的采样格式。
                                  resample_fmt_.sample_rate, //输出采样率
                                  &in_ch_layout, decode_ctx_->sample_fmt,
                                  decode_ctx_->sample_rate, 0, nullptr);
    if (ret < 0) {
        logger_->error("swr_alloc_set_opts2 fail, {}",
                       avutil::ErrorString(ret));
        return false;
    }

    /* create resampler context 方式2 */
    //    swr_ctx_ = swr_alloc();
    //    if (!swr_ctx_) {
    //        logger_->error("Could not allocate resampler context");
    //        return false;
    //    }

    /* set options */
    //    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &src_ch_layout, 0);
    //    av_opt_set_int(swr_ctx_, "in_sample_rate", src_rate, 0);
    //    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", src_sample_fmt, 0);
    //    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &dst_ch_layout, 0);
    //    av_opt_set_int(swr_ctx_, "out_sample_rate", dst_rate, 0);
    //    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx_)) < 0) {
        logger_->error("swr_init fail, {}", avutil::ErrorString(ret));
        return false;
    }

    return true;
}

bool AudioPlayer::ResampleFormatValid() const {
    return resample_fmt_.channel_count > 0 && resample_fmt_.sample_rate > 0 &&
           resample_fmt_.sample_fmt != AV_SAMPLE_FMT_NONE;
}
