#include "av_util.h"

extern "C" {
// #include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
// #include <libavformat/avformat.h>
// #include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
// #include <libavutil/timestamp.h>
};

#include <iostream>

namespace avutil {

std::string ErrorString(int err) {
    char buf[80] = {0};
    av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, err);
    return buf;
}

std::string GetPixFmtName(AVPixelFormat pix_fmt) {
    const char *name = av_get_pix_fmt_name(pix_fmt);
    if (name) {
        return std::string(name);
    } else {
        return std::string();
    }
}

std::string GetSampleFmtName(AVSampleFormat sample_fmt) {
    const char *name = av_get_sample_fmt_name(sample_fmt);
    if (name) {
        return std::string(name);
    } else {
        return std::string();
    }
}

std::string GetHWDeviceTypeName(enum AVHWDeviceType type) {
    auto name = av_hwdevice_get_type_name(type);
    if (name)
        return name;
    else
        return "none";
}

std::string GetCodecName(AVCodecID id) { return avcodec_get_name(id); }

void GetAllDevices() {
    // windows系统的输入格式为dshow
    const AVInputFormat *iFormat = av_find_input_format("dshow");
    AVDeviceInfoList *devList = nullptr;
    avdevice_list_input_sources(iFormat, nullptr, nullptr, &devList);
    if (devList != nullptr) {
        std::cout << " device count " << devList->nb_devices << std::endl;

        for (int i = 0; i < devList->nb_devices; i++) {
            AVDeviceInfo *devInfo = devList->devices[i];
            if (!devInfo) {
                std::cout << "device " << i << " is null" << std::endl;
                continue;
            }
            std::cout << "device " << i << std::endl;

            if (devInfo->device_description) {
                std::cout << "device desc " << devInfo->device_description
                          << std::endl;
            }
            if (devInfo->device_name) {
                std::cout << "device name " << devInfo->device_name
                          << std::endl;
            }

            if (!devInfo->media_types) {
                std::cout << "device " << i << " media type is null"
                          << std::endl;
                continue;
            } else {
                enum AVMediaType type = *devInfo->media_types;
            }
        }
    }
}

AVHWDeviceType GetDefaultHWDeviceType() {
#ifdef _WIN32
    return AV_HWDEVICE_TYPE_QSV;
#elif defined(__linux__)
    return AV_HWDEVICE_TYPE_VAAPI;
#else
    return AV_HWDEVICE_TYPE_VAAPI;
#endif
}

std::string GetDecoderSuffixByHWDeviceType(AVHWDeviceType type) {
    std::string suffix;
    switch (type) {
    case AV_HWDEVICE_TYPE_QSV:
        suffix = "_qsv";
        break;
    case AV_HWDEVICE_TYPE_CUDA:
        suffix = "_cuvid";
        break;
    default:
        break;
    }
    return suffix;
}

std::string ChannelLayoutDescribe(const AVChannelLayout *ch_layout) {
    char buf[100] = {0};
    if (av_channel_layout_describe(ch_layout, buf, 100) < 0) {
        return std::string();
    }
    return std::string(buf);
}

} // namespace avutil
