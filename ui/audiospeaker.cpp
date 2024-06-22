#include "audiospeaker.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QThread>

#define AUDIOSPEAKER_BUFFER_SIZE 18000

AudioSpeakerImpl::AudioSpeakerImpl() : buffer_size_(0), queued_clock_(0.0) {}

AudioSpeakerImpl::~AudioSpeakerImpl() {
    qDebug() << QThread::currentThreadId() << "~AudioSpeakerImpl()";
}

double AudioSpeakerImpl::audioClock() {
    //     buffer_size_ = format_.sampleRate() * format_.bytesPerSample()
    //     *format_.channelCount();
    //     1秒数据量。如果不是这个长度，需要修改计算公式
    if (buffer_size_ <= 0) {
        return 0;
    } else if (!audio_sink_) {
        return 0;
    } else {
        return queued_clock_ -
               double(buffer_size_ - audio_sink_->bytesFree()) / buffer_size_;
    }
}

int AudioSpeakerImpl::bytesFree() {
    if (audio_sink_) {
        return audio_sink_->bytesFree();
    } else {
        return AUDIOSPEAKER_BUFFER_SIZE;
    }
}

void AudioSpeakerImpl::slotStart() {
    QAudioDevice device = DefaultDevice();
    format_ = PreferredFormat(&device);

    qDebug() << QThread::currentThreadId() << "use audio device "
             << device.description() << ", format " << format_;

    audio_sink_.reset(new QAudioSink(device, format_));
    connect(audio_sink_.get(), &QAudioSink::stateChanged, this,
            &AudioSpeakerImpl::handleStateChanged);

    buffer_size_ = format_.sampleRate() * format_.bytesPerSample() *
                   format_.channelCount(); // 1秒数据量

    audio_sink_->setBufferSize(buffer_size_);
    audio_device_ = audio_sink_->start();

    qDebug() << QThread::currentThreadId()
             << "AudioSpeakerImpl::slotStart(), audio sink buf size:"
             << audio_sink_->bufferSize();
}

void AudioSpeakerImpl::slotStop() {
    audio_sink_->stop();
    audio_sink_.reset();
    audio_device_ = nullptr;
    qDebug() << QThread::currentThreadId() << "AudioSpeakerImpl::slotStop";
}

void AudioSpeakerImpl::slotPause() {
    if (audio_sink_)
        audio_sink_->suspend();
}

void AudioSpeakerImpl::slotResume() {
    if (audio_sink_)
        audio_sink_->resume();
}

void AudioSpeakerImpl::slotWrite(const char *data, int len, double clock) {
    if (!audio_device_) {
        return;
    }

    // write会触发QTimer，所以也必须在创建者同一个线程。
    audio_device_->write(data, len);
    queued_clock_ = clock;
    qDebug() << QThread::currentThreadId() << "queued frame clock " << clock
             << ", audio clock " << audioClock()
             << ", bytes free: " << audio_sink_->bytesFree();

    delete[] data;
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
             << audio_sink_->bufferSize() << ", bytes free "
             << audio_sink_->bytesFree();
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

AudioSpeaker::AudioSpeaker()
    : QObject(nullptr), workThread(nullptr), impl(nullptr) {}

AudioSpeaker::~AudioSpeaker() { stop(); }

void AudioSpeaker::start() {
    qDebug() << "AudioSpeaker::start()";

    workThread = new QThread;
    // 不能设置父对象
    impl = new AudioSpeakerImpl();
    impl->moveToThread(workThread);

    // 线程QThread也是类的对象，因此，可以通过槽函数和信号在对象间通信：
    // 在QThread对象中执行另一个需要循环执行的对象的成员函数（while循环）
    QObject::connect(workThread, &QThread::started, impl,
                     &AudioSpeakerImpl::slotStart);
    QObject::connect(workThread, &QThread::finished, impl,
                     &AudioSpeakerImpl::slotStop);
    QObject::connect(this, &AudioSpeaker::write, impl,
                     &AudioSpeakerImpl::slotWrite);
    QObject::connect(this, &AudioSpeaker::pause, impl,
                     &AudioSpeakerImpl::slotPause);
    QObject::connect(this, &AudioSpeaker::resume, impl,
                     &AudioSpeakerImpl::slotResume);
    workThread->start();

    qDebug() << "audio speaker impl is started";
}

void AudioSpeaker::stop() {
    if (workThread) {
        impl->deleteLater();
        workThread->quit();
        workThread->wait();
        delete workThread;
        workThread = nullptr;
    }
}

double AudioSpeaker::audioClock() {
    if (impl) {
        return impl->audioClock();
    } else {
        return 0;
    }
}

int AudioSpeaker::bytesFree() {
    if (impl) {
        return impl->bytesFree();
    } else {
        return AUDIOSPEAKER_BUFFER_SIZE;
    }
}
