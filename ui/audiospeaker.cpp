#include "audiospeaker.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QThread>

AudioSpeaker::AudioSpeaker()
    : QObject(nullptr), buffer_size_(0), queued_clock_(0.0) {
    connect(this, &AudioSpeaker::write, this, &AudioSpeaker::writeSlot);
}

AudioSpeaker::~AudioSpeaker() {
    if (audio_sink_) {
        Stop();
    }
}

void AudioSpeaker::Start() {
    QAudioDevice device = DefaultDevice();
    format_ = PreferredFormat(&device);

    qDebug() << QThread::currentThreadId() << "use audio device "
             << device.description() << ", format " << format_;

    audio_sink_.reset(new QAudioSink(device, format_));
    connect(audio_sink_.get(), &QAudioSink::stateChanged, this,
            &AudioSpeaker::handleStateChanged);

    buffer_size_ = format_.sampleRate() * format_.bytesPerSample() *
                   format_.channelCount(); // 1秒数据量

    audio_sink_->setBufferSize(buffer_size_);
    audio_device_ = audio_sink_->start();

    qDebug() << QThread::currentThreadId()
             << "audio sink buf size: " << audio_sink_->bufferSize();
}

void AudioSpeaker::Stop() {
    qDebug() << QThread::currentThreadId() << "run finished ";

    disconnect(audio_sink_.get(), &QAudioSink::stateChanged, this,
               &AudioSpeaker::handleStateChanged);

    audio_sink_->stop();
    audio_sink_.reset();
    audio_device_ = nullptr;
}

void AudioSpeaker::Pause() {
    // if (audio_sink_) {
    //     audio_sink_->suspend();
    // }
    emit this->pauseSignal();
}

void AudioSpeaker::Resume() {
    // if (audio_sink_) {
    //     audio_sink_->resume();
    // }
    emit this->resumeSignal();
}

void AudioSpeaker::pauseSlot() {
    if (audio_sink_) {
        audio_sink_->suspend();
    }
}

void AudioSpeaker::resumeSlot() {
    if (audio_sink_) {
        audio_sink_->resume();
    }
}

// void AudioSpeaker::stopSlot() {
//     qDebug() << QThread::currentThreadId() << " stopSlot";
//     requestInterruption();
//     cond_.notify_one();
// }

QAudioDevice AudioSpeaker::DefaultDevice() {
    return QMediaDevices::defaultAudioOutput();
}

QAudioFormat AudioSpeaker::PreferredFormat(QAudioDevice *device) {
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

void AudioSpeaker::writeSlot(const char *data, int len, double clock) {
    if (!audio_device_) {
        return;
    }

    // write会触发QTimer，所以也必须在创建者同一个线程。
    audio_device_->write(data, len);
    queued_clock_.store(clock);
    // qDebug() << "audio sink write " << wlen
    //          << ", bytes free: " << audio_sink_->bytesFree();
    qDebug() << QThread::currentThreadId() << "queued frame clock " << clock
             << ", audio clock " << AudioClock();

    delete[] data;
}

double AudioSpeaker::AudioClock() {
    //     buffer_size_ = format_.sampleRate() * format_.bytesPerSample()
    //     *format_.channelCount();
    //     1秒数据量。如果不是这个长度，需要修改计算公式
    if (buffer_size_ <= 0) {
        return 0;
    } else if (!audio_sink_) {
        return 0;
    } else {
        return queued_clock_.load() -
               double(buffer_size_ - audio_sink_->bytesFree()) / buffer_size_;
    }
}

int AudioSpeaker::bytesFree() {
    if (audio_sink_) {
        return audio_sink_->bytesFree();
    } else {
        return 0;
    }
}

void AudioSpeaker::handleStateChanged(QAudio::State newState) {
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

void AudioSpeaker::readyRead() { qDebug() << "readyRead()"; }

void AudioSpeaker::bytesWritten(qint64 bytes) {
    qDebug() << "bytesWritten " << bytes;
}

void AudioSpeaker::channelReadyRead(int channel) {
    qDebug() << "channelReadyRead " << channel;
}

void AudioSpeaker::channelBytesWritten(int channel, qint64 bytes) {
    qDebug() << "channelBytesWritten " << channel << ", bytes " << bytes;
}
