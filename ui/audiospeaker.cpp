#include "audiospeaker.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>

AudioSpeaker::AudioSpeaker(QObject *parent) : QThread{parent} {}

AudioSpeaker::~AudioSpeaker() {
    if (audio_sink_) {
        audio_sink_->stop();
        audio_sink_.reset();
    }
}

QAudioFormat AudioSpeaker::PreferredFormat() {
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice info(QMediaDevices::defaultAudioOutput());
    if (info.isFormatSupported(format)) {
        return format;
    } else {
        return info.preferredFormat();
    }
}

void AudioSpeaker::write(const char *data, int len, long long pts) {
    if (!audio_device_)
        return;

    AudioSpeakerFrame frame;
    frame.buf = data;
    frame.length = len;

    qDebug() << "enqueue frame " << len << ", pts " << pts;

    QMutexLocker<QMutex> lock(&mutex_);
    queue_.enqueue(frame);
    cond_.notify_one();
}

long long AudioSpeaker::AudioClock() { return 0; }

void AudioSpeaker::run() {
    QAudioFormat format = PreferredFormat();
    QAudioDevice info(QMediaDevices::defaultAudioOutput());

    // QAudioFormat format;
    // format.setSampleRate(44100);
    // format.setChannelCount(2);
    // format.setSampleFormat(QAudioFormat::Int16);

    // if (!info.isFormatSupported(format)) {
    //     qWarning()
    //         << "Raw audio format not supported by backend, cannot play
    //         audio.";
    //     return;
    // }

    qDebug() << "use audio device " << info.description() << ", format "
             << format;

    audio_sink_.reset(new QAudioSink(info, format));
    connect(audio_sink_.get(), &QAudioSink::stateChanged, this,
            &AudioSpeaker::handleStateChanged);

    long long buffer_size = format.sampleRate() * format.channelCount() *
                            format.bytesPerSample(); // 1秒数据量

    audio_sink_->setBufferSize(buffer_size);
    audio_device_ = audio_sink_->start();

    while (!isInterruptionRequested()) {
        mutex_.lock();
        while (queue_.empty() && !isInterruptionRequested()) {
            cond_.wait(&mutex_);
        }

        if (isInterruptionRequested()) {
            mutex_.unlock();
            break;
        }

        AudioSpeakerFrame frame_data = queue_.dequeue();
        mutex_.unlock();

        qDebug() << "dequeue frame " << frame_data.length;

        while (audio_sink_->bytesFree() < frame_data.length) {
            qDebug() << "audio sink buf size: " << audio_sink_->bufferSize()
                     << ", bytes free: " << audio_sink_->bytesFree()
                     << ", wait ";
            sleep(std::chrono::milliseconds(10));
        }

        qDebug() << "audio sink buf size: " << audio_sink_->bufferSize()
                 << ", bytes free: " << audio_sink_->bytesFree();
        auto wlen = audio_device_->write(frame_data.buf, frame_data.length);
        qDebug() << "write " << wlen
                 << ",  buf size: " << audio_sink_->bufferSize()
                 << ", bytes free: " << audio_sink_->bytesFree();

        delete[] frame_data.buf;
    }

    qDebug() << "run finished ";

    disconnect(audio_sink_.get(), &QAudioSink::stateChanged, this,
               &AudioSpeaker::handleStateChanged);

    audio_sink_->stop();
    audio_sink_.reset();
    audio_device_ = nullptr;
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
