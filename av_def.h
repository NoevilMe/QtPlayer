#ifndef AV_DEF_H
#define AV_DEF_H

#include <string>

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

struct VideoFrame {
    VideoFrame(int fmt, int w, int h) : pixfmt(fmt), width(w), height(h) {
        // 分配材质内存空间
        data[0] = new unsigned char[width * height]; // Y
        data[1] = new unsigned char[width * height / 2]; // U，NV12会将UV放这里
        data[2] = new unsigned char[width * height / 2]; // V
    }
    ~VideoFrame() {
        if (data[0]) {
            delete[] data[0];
            data[0] = nullptr;
        }

        if (data[1]) {
            delete[] data[1];
            data[1] = nullptr;
        }

        if (data[2]) {
            delete[] data[2];
            data[2] = nullptr;
        }
    }

    int pixfmt = -1;
    int width = 0;
    int height = 0;
    unsigned char *data[3] = {0};
};

#endif // AV_DEF_H
