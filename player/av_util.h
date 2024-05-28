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
std::string GetSampleFmtName(enum AVSampleFormat sample_fmt);
std::string GetHWDeviceTypeName(enum AVHWDeviceType type);
std::string GetCodecName(enum AVCodecID id);
std::string GetColorPrimariesName(enum AVColorPrimaries primaries);
AVHWDeviceType GetDefaultHWDeviceType();
std::string GetDecoderSuffixByHWDeviceType(AVHWDeviceType type);
std::string ChannelLayoutDescribe(const AVChannelLayout *ch_layout);
void GetAllDevices();
} // namespace avutil

#endif // AV_UTIL_H
