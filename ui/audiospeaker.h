#ifndef AUDIOSPEAKER_H
#define AUDIOSPEAKER_H

#include <QAudioSink>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

#include <atomic>

class AudioSpeakerImpl : public QObject {
    Q_OBJECT
public:
    AudioSpeakerImpl();
    ~AudioSpeakerImpl();

    // 内部释放data
    double audioClock();
    int bytesFree();

signals:
    void pause();
    void resume();
    void write(const char *data, int len, double clock);

public slots:
    void slotStart();
    void slotStop();
    void slotPause();
    void slotResume();
    void slotWrite(const char *data, int len, double clock);

    QAudioDevice DefaultDevice();
    QAudioFormat PreferredFormat(QAudioDevice *device = nullptr);

    void handleStateChanged(QAudio::State newState);

private:
    QAudioFormat format_;
    long long buffer_size_;

    double queued_clock_;

    QScopedPointer<QAudioSink> audio_sink_;
    QIODevice *audio_device_ = nullptr;
};

class AudioSpeaker : public QObject {
    Q_OBJECT
public:
    explicit AudioSpeaker(bool newThread = true);
    ~AudioSpeaker();

    void start();
    void stop();

    void pause();
    void resume();
    void write(const char *data, int len, double clock);

    // 内部释放data
    double audioClock();
    int bytesFree();

private:
    bool createThread;
    QThread *workThread;
    AudioSpeakerImpl *impl;
    std::atomic_bool sendingData;
};

#endif // AUDIOSPEAKER_H
