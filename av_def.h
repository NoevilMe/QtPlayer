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

#endif // AV_DEF_H
