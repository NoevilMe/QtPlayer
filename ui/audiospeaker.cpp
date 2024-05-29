#include "audiospeaker.h"

#include <QDebug>
#include <QMediaDevices>
#include <QMutexLocker>

AudioSpeaker::AudioSpeaker(QObject *parent)
    : QThread{parent}, buffer_size_(0), queued_clock_(0.0) {}

AudioSpeaker::~AudioSpeaker() {
    if (audio_sink_) {
        Stop();
    }
}

void AudioSpeaker::Stop() {
    requestInterruption();
    cond_.notify_one();
}

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

void AudioSpeaker::write(const char *data, int len, double clock) {
    if (!audio_device_)
        return;

    AudioSpeakerFrame frame;
    frame.buf = data;
    frame.length = len;
    frame.clock = clock;

    QMutexLocker<QMutex> lock(&mutex_);
    queue_.enqueue(frame);
    cond_.notify_one();
}

double AudioSpeaker::AudioClock() {
    //     buffer_size_ = format_.sampleRate() * format_.bytesPerSample()
    //     *format_.channelCount();
    //     1秒数据量。如果不是这个长度，需要修改计算公式
    return queued_clock_.load() -
           double(buffer_size_ - audio_sink_->bytesFree()) / buffer_size_;
}

void AudioSpeaker::run() {
    QAudioDevice device = DefaultDevice();
    format_ = PreferredFormat(&device);

    qDebug() << "use audio device " << device.description() << ", format "
             << format_;

    audio_sink_.reset(new QAudioSink(device, format_));
    connect(audio_sink_.get(), &QAudioSink::stateChanged, this,
            &AudioSpeaker::handleStateChanged);

    buffer_size_ = format_.sampleRate() * format_.bytesPerSample() *
                   format_.channelCount(); // 1秒数据量

    audio_sink_->setBufferSize(buffer_size_);
    audio_device_ = audio_sink_->start();

    qDebug() << "audio sink buf size: " << audio_sink_->bufferSize();

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

        while (audio_sink_->bytesFree() < frame_data.length) {
            // qDebug() << "audio sink bytes free: " << audio_sink_->bytesFree()
            //          << ", wait ";
            sleep(std::chrono::milliseconds(5));
        }

        // qDebug() << "audio sink bytes free: " << audio_sink_->bytesFree();
        auto wlen = audio_device_->write(frame_data.buf, frame_data.length);
        queued_clock_.store(frame_data.clock);
        // qDebug() << "audio sink write " << wlen
        //          << ", bytes free: " << audio_sink_->bytesFree();

        qDebug()<<"queued frame clock "<<frame_data.clock << ", audio clock " << AudioClock();

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
