#ifndef AV_UTIL_H
#define AV_UTIL_H

#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

namespace avutil {
std::string ErrorString(int err);
std::string GetPixFmtName(enum AVPixelFormat pix_fmt);
std::string GetHWDeviceTypeName(enum AVHWDeviceType type);
std::string GetCodecName(enum AVCodecID id);
AVHWDeviceType GetDefaultHWDeviceType();
std::string GetDecoderSuffixByHWDeviceType(AVHWDeviceType type);
void GetAllDevices();
} // namespace avutil

#endif // AV_UTIL_H
