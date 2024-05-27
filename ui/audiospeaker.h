#ifndef AUDIOSPEAKER_H
#define AUDIOSPEAKER_H

#include <QAudioSink>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

class AudioSpeaker : public QThread {
    Q_OBJECT
public:
    explicit AudioSpeaker(QObject *parent = nullptr);
    ~AudioSpeaker();

    QAudioFormat PreferredFormat();

    void write(const char *data, int len, long long pts);
    long long AudioClock();

    // QThread interface
protected:
    struct AudioSpeakerFrame {
        const char *buf;
        int length;
        long long pts;
    };

    void run() override;

    void handleStateChanged(QAudio::State newState);
    void readyRead();
    void bytesWritten(qint64 bytes);
    void channelReadyRead(int channel);
    void channelBytesWritten(int channel, qint64 bytes);

private:
    QAudioFormat preferred_format_;

    QQueue<AudioSpeakerFrame> queue_;
    QMutex mutex_;
    QWaitCondition cond_;

    QScopedPointer<QAudioSink> audio_sink_;
    QIODevice *audio_device_ = nullptr;
};

#endif // AUDIOSPEAKER_H
