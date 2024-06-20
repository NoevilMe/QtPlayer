#ifndef AUDIOSPEAKER_H
#define AUDIOSPEAKER_H

#include <QAudioSink>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

class AudioSpeaker : public QThread {
    Q_OBJECT
public:
    explicit AudioSpeaker(QObject *parent = nullptr);
    ~AudioSpeaker();

    void Stop();
    void Pause();
    void Resume();

    QAudioDevice DefaultDevice();
    QAudioFormat PreferredFormat(QAudioDevice *device = nullptr);

    // 内部释放data
    void write(const char *data, int len, double clock);
    double AudioClock();

signals:
    void pauseSignal();
    void resumeSignal();

private slots:
    void pauseSlot();
    void resumeSlot();

    // QThread interface
protected:
    struct AudioSpeakerFrame {
        const char *buf;
        int length;
        double clock;
    };

    void run() override;

    void handleStateChanged(QAudio::State newState);
    void readyRead();
    void bytesWritten(qint64 bytes);
    void channelReadyRead(int channel);
    void channelBytesWritten(int channel, qint64 bytes);

private:
    QAudioFormat format_;
    long long buffer_size_;

    std::atomic<double> queued_clock_;

    QQueue<AudioSpeakerFrame> queue_;
    QMutex mutex_;
    QWaitCondition cond_;

    QScopedPointer<QAudioSink> audio_sink_;
    QIODevice *audio_device_ = nullptr;
};

#endif // AUDIOSPEAKER_H
