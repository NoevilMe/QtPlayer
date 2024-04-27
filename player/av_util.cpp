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
    return av_get_pix_fmt_name(pix_fmt);
}

std::string GetHwDeviceTypeName(enum AVHWDeviceType type) {
    auto name = av_hwdevice_get_type_name(type);
    if (name)
        return name;
    else
        return std::string();
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

} // namespace avutil
