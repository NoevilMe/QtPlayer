#include "av_util.h"

extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavdevice/avdevice.h>
// #include <libavformat/avformat.h>
// #include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
// #include <libavutil/timestamp.h>
};

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

} // namespace avutil
