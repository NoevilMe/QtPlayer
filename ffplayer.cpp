#include "ffplayer.h"
#include "av_util.h"

extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavdevice/avdevice.h>
// #include <libavformat/avformat.h>
// #include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
// #include <libavutil/timestamp.h>
};

#define DSHOW_TIMEOUT_MS 10000

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
                    avutil::GetHwDeviceTypeName(config->device_type),
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

FFPlayer::FFPlayer() {
    logger_ = util::log::GetLogger(__func__);
    g_av_logger_ = logger_;

    decode_frame_ = av_frame_alloc();
}

FFPlayer::~FFPlayer() {
    if (decode_frame_) {
        av_frame_free(&decode_frame_);
        decode_frame_ = nullptr;
    }
}

bool FFPlayer::Start() {
    if (!InitInputContext()) {
        return false;
    }

    // decoder
    std::string codec_name =
        avutil::GetCodecName(input_video_stream_->codecpar->codec_id);
    codec_name += "_qsv";

    const AVCodec *input_codec =
        avcodec_find_decoder_by_name(codec_name.data());
    if (!input_codec) {
        logger_->error("can not find decoder {}", codec_name);
        return false;
    }
    logger_->info("input codec {}:{}", input_codec->name,
                  input_codec->long_name);
    hwtype_ = AV_HWDEVICE_TYPE_QSV;

    if (!InitHWDeviceContext(input_codec)) {
        return false;
    }

    if (!InitInputDecodeContext(input_codec)) {
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

bool FFPlayer::InitInputContext() {
    const AVInputFormat *input_fmt = nullptr;

    input_fmt = av_find_input_format("dshow");
    if (input_fmt == nullptr) {
        logger_->error("can not find dshow");
        return false;
    }

    std::string dev("video=HIK 1080P Camera");

    input_fmt_ctx_ = avformat_alloc_context();

    if (!input_fmt_ctx_) {
        logger_->error("avformat_alloc_context fail");
        return false;
    }

    input_fmt_ctx_->flags |= AVFMT_FLAG_NONBLOCK; // 拔掉摄像头不阻塞

    // set input options
    AVDictionary *options = nullptr;
    // av_dict_set(&options, "fflags", "nobuffer", 0);
    // av_dict_set(&options, "max_delay", "100000", 0);
    // av_dict_set(&options, "framerate", "30", 0);
    // av_dict_set(&options, "probesize", "100000000", 0);
    // av_dict_set(&options, "analyzeduration", "5000000", 0);

    // framerate needs to set before opening the v4l2 device
    //   av_dict_set(&options, "framerate", "15", 0);
    // This will not work if the camera does not support h264. In that case
    // remove this line. I wrote this for Raspberry Pi where the camera driver
    // can stream h264.
    // av_dict_set(&options, "input_format", "h264", 0);
    // av_dict_set(&options, "pixel_format", "yuvj420p", 0);
    av_dict_set(&options, "input_format", "mjpeg", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);
    // av_dict_set(&options, "pixel_format", "rgb24", 0);

    // av_dict_set(&options, "pixel_format", "yuyv422", 0);
    // https://superuser.com/questions/1310236/tell-ffmpeg-to-drop-frames-to-reduce-memory-usage
    // av_dict_set_int(&options, "rtbufsize", 18432000, 0);
    // av_dict_set (& options, "stimeout", "10000000", 0);//Set timeout
    // disconnect time

    util::AtExit ao([&]() { av_dict_free(&options); });

    input_fmt_ctx_->interrupt_callback.opaque = this;
    input_fmt_ctx_->interrupt_callback.callback = InterruptCallback;

    interrupt_.func_start_timestamp = util::TimeMilliseconds();

    auto err =
        avformat_open_input(&input_fmt_ctx_, dev.data(), input_fmt, &options);
    if (err) {
        logger_->error("avformat_open_input device {}, {}, {}", dev, err,
                       avutil::ErrorString(err));
        return false;
    }

    if (interrupt_.interrupted) {
        logger_->error("can't open input device {}, timeout", dev);
        return false;
    }

    logger_->debug("avformat_open_input success");

    // 1.2 解码一段数据，获取流相关信息
    input_fmt_ctx_->probesize = 1000 * 1024;
    input_fmt_ctx_->max_analyze_duration = 5 * AV_TIME_BASE;

    interrupt_.func_start_timestamp = util::TimeMilliseconds();
    if (avformat_find_stream_info(input_fmt_ctx_, 0) < 0) {
        logger_->error("failed to retrieve input stream information");
        return false;
    }

    if (interrupt_.interrupted) {
        logger_->error("can't avformat_find_stream_info, timeout");
        return false;
    }

    logger_->debug("avformat_find_stream_info success");

    // 1.3 获取输入ctx
    int videoIndex = -1;
    for (int i = 0; i < input_fmt_ctx_->nb_streams; ++i) {
        if (input_fmt_ctx_->streams[i]->codecpar->codec_type ==
            AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }

    if (videoIndex == -1) {
        logger_->error("no video stream in input stream");
        return false;
    }

    input_video_stream_ = input_fmt_ctx_->streams[videoIndex];
    logger_->info("input streams video index = {}, avg fps is {}, codec id {}",
                  videoIndex, input_video_stream_->avg_frame_rate.num,
                  input_video_stream_->codecpar->codec_id);

    // 输出调试信息：tbr代表帧率；tbn代表文件层（st）的时间精度，即1S=1200k，和duration相关；tbc代表视频层（st->codec）的时间精度，即1S=XX，和stream->duration和时间戳相关。
    //  TODO:
    std::string name(fmt::format("@ {}", dev));
    av_dump_format(input_fmt_ctx_, videoIndex, name.data(), 0);
    return true;
}

bool FFPlayer::InitHWDeviceContext(const AVCodec *codec, bool get_hw_type) {
    if (get_hw_type) {
        auto hwconfig = AvUtilGetHwConfig(codec, hwtype_);
        if (!hwconfig) {
            logger_->error("can not get hwaccel {} config for {}",
                           avutil::GetHwDeviceTypeName(hwtype_),
                           codec->long_name);
            return false;
        }

        hw_pix_fmt_ = hwconfig->pix_fmt;

        logger_->info("apply hw config [{}, {}] for codec {}",
                      avutil::GetHwDeviceTypeName(hwconfig->device_type),
                      avutil::GetPixFmtName(hwconfig->pix_fmt), codec->name);
        int err = av_hwdevice_ctx_create(&hw_device_ctx_, hwconfig->device_type,
                                         nullptr, nullptr, 0);
        if (err) {
            logger_->error("failed to av_hwdevice_ctx_create, {}",
                           avutil::ErrorString(err));
            return false;
        }
    } else {
        int err =
            av_hwdevice_ctx_create(&hw_device_ctx_, hwtype_, nullptr, // "auto"
                                   nullptr, 0);
        if (err) {
            logger_->error("failed to av_hwdevice_ctx_create, {}",
                           avutil::ErrorString(err));
            return false;
        }

        hw_pix_fmt_ = AV_PIX_FMT_QSV;

        logger_->debug("use hw pix fmt {}", avutil::GetPixFmtName(hw_pix_fmt_));
    }

    logger_->debug("InitHWDeviceContext success");

    return true;
}

bool FFPlayer::InitInputDecodeContext(const AVCodec *dec) {
    input_decode_ctx_ = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(input_decode_ctx_,
                                  input_video_stream_->codecpar);

    logger_->debug("input stream time_base {}, {}",
                   input_video_stream_->time_base.num,
                   input_video_stream_->time_base.den);
    logger_->debug("input stream avg_frame_rate {}, {}",
                   input_video_stream_->avg_frame_rate.num,
                   input_video_stream_->avg_frame_rate.den);

    input_decode_ctx_->time_base = input_video_stream_->time_base;
    input_decode_ctx_->framerate = input_video_stream_->avg_frame_rate;

    logger_->info("input decoder time_base {}, {}",
                  input_decode_ctx_->time_base.num,
                  input_decode_ctx_->time_base.den);
    logger_->info("input decoder framerate {}, {}",
                  input_decode_ctx_->framerate.num,
                  input_decode_ctx_->framerate.den);

    input_decode_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    input_decode_ctx_->opaque = this;
    input_decode_ctx_->get_format = GetFormat;

    AVDictionary *codec_opts = nullptr;

    util::AtExit e([&]() {
        if (codec_opts) {
            av_dict_free(&codec_opts);
        }
    });

    if (input_decode_ctx_->codec_type == AVMEDIA_TYPE_VIDEO ||
        input_decode_ctx_->codec_type == AVMEDIA_TYPE_AUDIO) {
        av_dict_set(&codec_opts, "refcounted_frames", "1", 0);
    }

    int err = avcodec_open2(input_decode_ctx_, dec, NULL);
    if (err < 0) {
        return false;
        logger_->error("failed to avcodec_open2, {}", avutil::ErrorString(err));
        return false;
    }

    input_video_stream_->discard = AVDISCARD_DEFAULT;

    logger_->info("src fmt {}, resolution {} x {}",
                  avutil::GetPixFmtName(input_decode_ctx_->pix_fmt),
                  input_decode_ctx_->width, input_decode_ctx_->height);

    logger_->debug("InitInputDecodeContext success");
    return true;
}

bool FFPlayer::HandleInputFrame(AVPacket *pkt) {
    if (!pkt || pkt->size <= 0)
        return false;

    static int count = 1;
    pkt->dts = pkt->pts = count++;
    pkt->duration = 1;

    int ret = avcodec_send_packet(input_decode_ctx_, pkt);
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
        ret = avcodec_receive_frame(input_decode_ctx_, decode_frame_);

        if (pkt && ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            logger_->error("avcodec_receive_frame yuv frame failure, {}",
                           avutil::ErrorString(ret));
            return false;
        }

        util::AtExit r([&]() { av_frame_unref(decode_frame_); });

        logger_->trace(
            "avcodec_receive_frame ok, fmt {}, resolution {}x{}",
            avutil::GetPixFmtName((AVPixelFormat)decode_frame_->format),
            decode_frame_->width, decode_frame_->height);
    }

    return true;
}

AVPixelFormat FFPlayer::GetFormat(AVCodecContext *ctx,
                                  const AVPixelFormat *pix_fmts) {
    FFPlayer *inst = static_cast<FFPlayer *>(ctx->opaque);
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == inst->hw_pix_fmt_) {
            return *p;
        }
    }

    inst->logger_->error("Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

int FFPlayer::InterruptCallback(void *context) {
    FFPlayer *obj = (FFPlayer *)context;
    obj->interrupt_.func_end_timestamp = util::TimeMilliseconds();
    if (obj->interrupt_.func_end_timestamp -
            obj->interrupt_.func_start_timestamp >
        DSHOW_TIMEOUT_MS) {
        obj->logger_->error("device timeout {} ms", DSHOW_TIMEOUT_MS);
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
            err = av_read_frame(input_fmt_ctx_, pkt);
            if (0 == err) {
                // pkt->time_base = {1, 10000000};
                logger_->trace("av_read_frame ok, size {}, timebase {}, {}",
                               pkt->size, pkt->time_base.num,
                               pkt->time_base.den);

                this->HandleInputFrame(pkt);

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
