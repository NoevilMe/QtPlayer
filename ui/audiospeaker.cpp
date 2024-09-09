#include "audiospeaker.h"
#include "util/util.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QThread>

#define AUDIOSPEAKER_BUFFER_SIZE 18000

AudioSpeaker::AudioSpeaker(bool createThread)
    : QObject(nullptr),
      createThread(createThread),
      oldThread(QThread::currentThread()),
      newThread(nullptr),
      sendingData(false) {

    // 直接调用接口
    QObject::connect(this, &AudioSpeaker::resume, this,
                     &AudioSpeaker::slotResumeSink);
    QObject::connect(this, &AudioSpeaker::setVolume, this,
                     &AudioSpeaker::slotSetVolume);

    // 转调接口
    QObject::connect(this, &AudioSpeaker::pauseSink, this,
                     &AudioSpeaker::slotPauseSink);

    QObject::connect(this, &AudioSpeaker::writeSink, this,
                     &AudioSpeaker::slotWriteSink);
}

AudioSpeaker::~AudioSpeaker() {
    stop();
    qDebug() << QThread::currentThreadId() << "~AudioSpeakerImpl()";
}

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
    device = dev;
    format = fmt;

    if (createThread) {
        oldThread = QThread::currentThread();
        newThread = new QThread;
        this->moveToThread(newThread);

        // 线程QThread也是类的对象，因此，可以通过槽函数和信号在对象间通信：
        // 在QThread对象中执行另一个需要循环执行的对象的成员函数（while循环）
        QObject::connect(newThread, &QThread::started, this,
                         &AudioSpeaker::slotStartSink);
        QObject::connect(newThread, &QThread::finished, this,
                         &AudioSpeaker::slotStopSink);
        newThread->start();
    } else {
        this->slotStartSink();
    }

    qDebug() << "audio speaker impl is started";
}

void AudioSpeaker::stop() {
    if (newThread) {
        newThread->quit();
        newThread->wait();

        this->moveToThread(oldThread);
        delete newThread;
        newThread = nullptr;
    } else {
        this->slotStopSink();
    }
}

void AudioSpeaker::pause() {
    if (audioSink) {
        // sendingData为真的时候可能在等待Speaker可用空间足量，贸然暂停可能会导致死循环
        while (sendingData.load()) {
            qDebug() << "wait speaker bytesFree enough";
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        emit this->pauseSink();
    }
}

void AudioSpeaker::write(const std::shared_ptr<std::string> &data,
                         double clock) {
    if (!audioSink)
        return;

    qDebug() << QThread::currentThreadId() << util::TimeMilliseconds()
             << "AudioSpeaker::write" << clock << "size" << data->length();

    long long freeLength = (long long)data->length() * 2;

    // FIXME：
    // 如果调用write时候不延迟,QAudioSink播放速度会加快，可能导致了丢包。
    // 所以应该尽量减少在这里等待，

    sendingData.store(true);
    // 这在音频线程执行，但是impl可能会被其他线程停止掉, 所以要检测有效性
    // 实际测试下来需要有一定冗余空间，否则AudioSink可能会吞噬掉一些数据。这里设置为2倍
    while (audioSink && audioSink->bytesFree() < freeLength) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 这里信号槽可能会导致跨线程
    emit this->writeSink(data, clock);

    sendingData.store(false);
}

double AudioSpeaker::audioClock() {
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

int AudioSpeaker::bytesFree() {
    if (audioSink)
        return audioSink->bytesFree();
    else
        return 0;
}

void AudioSpeaker::slotStartSink() {
    qDebug() << QThread::currentThreadId() << "use audio device "
             << device.description() << ", format " << format;

    audioSink.reset(new QAudioSink(device, format));
    connect(audioSink.get(), &QAudioSink::stateChanged, this,
            &AudioSpeaker::handleStateChanged);

    // 设定缓存大小
    bufferSize = format.sampleRate() * format.bytesPerSample() *
                 format.channelCount(); // 1秒数据量
    audioSink->setBufferSize(bufferSize);

    audioDevice = audioSink->start();

    qDebug() << QThread::currentThreadId()
             << "AudioSpeaker::slotStartSink(), audio sink buf size:"
             << audioSink->bufferSize();
}

void AudioSpeaker::slotStopSink() {
    if (audioSink) {
        audioSink->stop();
        disconnect(audioSink.get(), &QAudioSink::stateChanged, this,
                   &AudioSpeaker::handleStateChanged);
        audioSink.reset();
        audioDevice = nullptr;
    }
    qDebug() << QThread::currentThreadId() << "AudioSpeaker::slotStopSink";
}

void AudioSpeaker::slotPauseSink() {
    if (audioSink)
        audioSink->suspend();
}

void AudioSpeaker::slotResumeSink() {
    if (audioSink) {
        audioSink->resume();
    }
}

void AudioSpeaker::slotWriteSink(const std::shared_ptr<std::string> &data,
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

void AudioSpeaker::slotSetVolume(int vol) {
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

void AudioSpeaker::handleStateChanged(QAudio::State newState) {
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
