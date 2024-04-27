#ifndef DEVICE_H
#define DEVICE_H

#include <vector>

struct HDevice {
    char name[256];
    //...
};

std::vector<HDevice> getVideoDevices();
std::vector<HDevice> getAudioDevices();


#endif // DEVICE_H
