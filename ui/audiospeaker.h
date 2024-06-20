#ifndef AUDIOSPEAKER_H
#define AUDIOSPEAKER_H

#include <QAudioSink>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

#include <atomic>

class AudioSpeaker : public QObject {
    Q_OBJECT
public:
    explicit AudioSpeaker();
    ~AudioSpeaker();

    void Start();
    void Stop();
    void Pause();
    void Resume();

    QAudioDevice DefaultDevice();
    QAudioFormat PreferredFormat(QAudioDevice *device = nullptr);

    // 内部释放data

    double AudioClock();
    int bytesFree();

signals:
    void pauseSignal();
    void resumeSignal();
    void write(const char *data, int len, double clock);

private slots:
    void pauseSlot();
    void resumeSlot();
    void writeSlot(const char *data, int len, double clock);

    // QThread interface
protected:
    struct AudioSpeakerFrame {
        const char *buf;
        int length;
        double clock;
    };

    void handleStateChanged(QAudio::State newState);
    void readyRead();
    void bytesWritten(qint64 bytes);
    void channelReadyRead(int channel);
    void channelBytesWritten(int channel, qint64 bytes);

private:
    QAudioFormat format_;
    long long buffer_size_;

    std::atomic<double> queued_clock_;

    QScopedPointer<QAudioSink> audio_sink_;
    QIODevice *audio_device_ = nullptr;
};

#endif // AUDIOSPEAKER_H
