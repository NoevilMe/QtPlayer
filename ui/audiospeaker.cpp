#include "audiospeaker.h"
#include "util/util.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QThread>

#define AUDIOSPEAKER_BUFFER_SIZE 18000

AudioSpeakerImpl::AudioSpeakerImpl(QAudioDevice dev, QAudioFormat fmt)
    : device(dev), format(fmt), bufferSize(0), queuedClock(0.0) {

    QObject::connect(this, &AudioSpeakerImpl::write, this,
                     &AudioSpeakerImpl::slotWrite);
    QObject::connect(this, &AudioSpeakerImpl::pause, this,
                     &AudioSpeakerImpl::slotPause);
    QObject::connect(this, &AudioSpeakerImpl::resume, this,
                     &AudioSpeakerImpl::slotResume);
    QObject::connect(this, &AudioSpeakerImpl::setVolume, this,
                     &AudioSpeakerImpl::slogSetVolume);
}

AudioSpeakerImpl::~AudioSpeakerImpl() {
    qDebug() << QThread::currentThreadId() << "~AudioSpeakerImpl()";
}

double AudioSpeakerImpl::audioClock() {
    // buffer_size_ = format_.sampleRate() * format_.bytesPerSample()
    //   *format_.channelCount();
    // buffer_size_保存的1秒数据量。如果不是这个长度，需要修改计算公式
    if (bufferSize <= 0) {
        return 0;
    } else if (!audioSink) {
        return 0;
    } else {
        return queuedClock -
               double(bufferSize - audioSink->bytesFree()) / bufferSize;
    }
}

int AudioSpeakerImpl::bytesFree() {
    if (audioSink) {
        return audioSink->bytesFree();
    } else {
        return AUDIOSPEAKER_BUFFER_SIZE;
    }
}

void AudioSpeakerImpl::slotStart() {
    qDebug() << QThread::currentThreadId() << "use audio device "
             << device.description() << ", format " << format;

    audioSink.reset(new QAudioSink(device, format));
    connect(audioSink.get(), &QAudioSink::stateChanged, this,
            &AudioSpeakerImpl::handleStateChanged);

    // 设定缓存大小
    bufferSize = format.sampleRate() * format.bytesPerSample() *
                 format.channelCount(); // 1秒数据量
    audioSink->setBufferSize(bufferSize);

    audioDevice = audioSink->start();

    qDebug() << QThread::currentThreadId()
             << "AudioSpeakerImpl::slotStart(), audio sink buf size:"
             << audioSink->bufferSize();
}

void AudioSpeakerImpl::slotStop() {
    if (audioSink) {
        audioSink->stop();
        disconnect(audioSink.get(), &QAudioSink::stateChanged, this,
                   &AudioSpeakerImpl::handleStateChanged);
        audioSink.reset();
        audioDevice = nullptr;
    }
    qDebug() << QThread::currentThreadId() << "AudioSpeakerImpl::slotStop";
}

void AudioSpeakerImpl::slotPause() {
    if (audioSink)
        audioSink->suspend();
}

void AudioSpeakerImpl::slotResume() {
    if (audioSink)
        audioSink->resume();
}

void AudioSpeakerImpl::slotWrite(const std::shared_ptr<std::string> &data,
                                 double clock) {
    if (!audioDevice) {
        return;
    }

    // write会触发QTimer，所以也必须在创建者同一个线程。
    audioDevice->write(data->data(), data->length());
    queuedClock = clock;
    qDebug() << QThread::currentThreadId() << util::TimeMilliseconds()
             << "queued frame clock" << clock << ", audio clock" << audioClock()
             << ", size" << data->size()
             << ", bytes free:" << audioSink->bytesFree();
}

void AudioSpeakerImpl::slogSetVolume(int vol) {
    if (!audioSink)
        return;

    if (vol <= 0) {
        audioSink->setVolume(0.0f);
    } else if (vol >= 100) {
        audioSink->setVolume(1.0f);
    } else {
        double volume = (double)vol / 100;
        audioSink->setVolume(volume);
    }
}

QAudioDevice AudioSpeakerImpl::DefaultDevice() {
    return QMediaDevices::defaultAudioOutput();
}

QAudioFormat AudioSpeakerImpl::PreferredFormat(QAudioDevice *device) {
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    if (device) {
        if (device->isFormatSupported(format)) {
            return format;
        } else {
            return device->preferredFormat();
        }
    } else {
        QAudioDevice info(QMediaDevices::defaultAudioOutput());
        if (info.isFormatSupported(format)) {
            return format;
        } else {
            return info.preferredFormat();
        }
    }
}

void AudioSpeakerImpl::handleStateChanged(QAudio::State newState) {
    qDebug() << "handleStateChanged " << newState << ", buffer size "
             << audioSink->bufferSize() << ", bytes free "
             << audioSink->bytesFree();
    switch (newState) {
    case QAudio::IdleState:
        // Finished playing (no more data)
        //        AudioOutputExample::stopAudioOutput();
        break;

    case QAudio::StoppedState:
        // Stopped for other reasons
        //        if (audio->error() != QAudio::NoError) {
        //            // Error handling
        //        }
        break;

    default:
        // ... other cases as appropriate
        break;
    }
}

AudioSpeaker::AudioSpeaker(bool newThread)
    : QObject(nullptr),
      createThread(newThread),
      workThread(nullptr),
      impl(nullptr),
      sendingData(false) {}

AudioSpeaker::~AudioSpeaker() { stop(); }

QAudioDevice AudioSpeaker::getDevice(const QString &desc) {
    /*
audio output id  "{0.0.0.00000000}.{68b4d037-fc80-4d90-bc1b-8b9e0d17e48e}" ,
desc  "耳机 (Realtek(R) Audio)" , preferred fmt  QAudioFormat( 48000 Hz,  2
Channels,  Float Format ) , sample fmts  QList(UInt8, Int16, Float) , sample
rate [ 11025 ,  96000 ], channel [ 1 ,  2 ] audio output id
"{0.0.0.00000000}.{4f438e8e-5bbd-4fd5-ac5a-9102d3ef0572}" , desc  "扬声器
(Realtek(R) Audio)" , preferred fmt  QAudioFormat( 48000 Hz,  2 Channels,  Float
Format ) , sample fmts  QList(UInt8, Int16, Float) , sample rate [ 11025 , 96000
], channel [ 1 ,  2 ]
     */

    QAudioDevice default_dev;
    auto audio_devics = QMediaDevices::audioOutputs();
    for (auto &dev : audio_devics) {
        qDebug() << "audio output id " << dev.id() << ", desc "
                 << dev.description() << ", preferred fmt "
                 << dev.preferredFormat() << ", sample fmts "
                 << dev.supportedSampleFormats() << ", sample rate ["
                 << dev.minimumSampleRate() << ", " << dev.maximumSampleRate()
                 << "], channel [" << dev.minimumChannelCount() << ", "
                 << dev.maximumChannelCount() << "]";

        if (dev.isDefault()) {
            // if (desc.isEmpty()) {
            //     return dev;
            // }else {
            //     default_dev = dev;
            // }

            default_dev = dev;
        } else if (dev.description() == desc) {
            return dev;
        }
    }

    return default_dev;
}

void AudioSpeaker::start(QAudioDevice dev, QAudioFormat fmt) {
    qDebug() << "AudioSpeaker::start()";

    // 不能设置父对象
    impl = new AudioSpeakerImpl(dev, fmt);

    if (createThread) {
        workThread = new QThread;
        impl->moveToThread(workThread);

        // 线程QThread也是类的对象，因此，可以通过槽函数和信号在对象间通信：
        // 在QThread对象中执行另一个需要循环执行的对象的成员函数（while循环）
        QObject::connect(workThread, &QThread::started, impl,
                         &AudioSpeakerImpl::slotStart);
        QObject::connect(workThread, &QThread::finished, impl,
                         &AudioSpeakerImpl::slotStop);
        workThread->start();
    } else {
        impl->slotStart();
    }

    qDebug() << "audio speaker impl is started";
}

void AudioSpeaker::stop() {
    if (workThread) {
        impl->deleteLater();
        workThread->quit();
        workThread->wait();
        delete workThread;
        workThread = nullptr;
        impl = nullptr;
    } else if (impl) {
        impl->slotStop();
        impl->deleteLater();
        impl = nullptr;
    }
}

void AudioSpeaker::pause() {
    if (impl) {
        // sendingData为真的时候可能在等待Speaker可用空间足量，贸然暂停可能会导致死循环
        while (sendingData.load()) {
            qDebug() << "wait speaker bytesFree enough";
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        emit impl->pause();
    }
}

void AudioSpeaker::resume() {
    if (impl) {
        emit impl->resume();
    }
}

void AudioSpeaker::write(const std::shared_ptr<std::string> &data,
                         double clock) {
    if (!impl)
        return;

    sendingData.store(true);
    // 这在音频线程执行，但是impl可能会被其他线程停止掉
    while (impl && impl->bytesFree() < data->length()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // 这里信号槽可能会导致跨线程，具体看impl实现
    if (impl) {
        emit impl->write(data, clock);
    }
    sendingData.store(false);
}

double AudioSpeaker::audioClock() {
    if (impl) {
        return impl->audioClock();
    } else {
        return 0;
    }
}

void AudioSpeaker::setVolume(int vol) {
    if (impl) {
        emit impl->setVolume(vol);
    }
}

int AudioSpeaker::bytesFree() {
    if (impl) {
        return impl->bytesFree();
    } else {
        return AUDIOSPEAKER_BUFFER_SIZE;
    }
}
