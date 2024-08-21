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

    // 当前时间点
    double audioClock();
    // 可用缓存空间
    int bytesFree();

signals:
    void pause();
    void resume();
    void write(const char *data, int len, double clock);

public slots:
    // 随线程外部绑定，或者直接调用
    void slotStart();
    void slotStop();
    // 内部绑定
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

    // 开始播放
    void start();
    // 停止播放
    void stop();
    // 暂停
    void pause();
    // 恢复
    void resume();

    // 写入音频数据
    void write(const char *data, int len, double clock);
    // 当前可用缓存空间
    int bytesFree();

    // 获取音频当前的时间点，double秒数
    double audioClock();

private:
    bool createThread;
    QThread *workThread;
    AudioSpeakerImpl *impl;
    std::atomic_bool sendingData;
};

#endif // AUDIOSPEAKER_H
