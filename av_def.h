#ifndef AV_DEF_H
#define AV_DEF_H

#include <string>

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

enum class AudioSampleFormat : unsigned short {
    Unknown,
    UInt8,
    Int16,
    Int32,
    Float,
    NSampleFormats
};

struct AudioDeviceFormat {
    int sample_rate = 0;
    AudioSampleFormat sample_fmt = AudioSampleFormat::Unknown;
    int channel_count = 0;
};

#endif // AV_DEF_H
