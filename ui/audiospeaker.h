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
    AudioSpeakerImpl(QAudioDevice dev, QAudioFormat fmt);
    ~AudioSpeakerImpl();

    // 当前时间点
    double audioClock();
    // 可用缓存空间
    int bytesFree();

signals:
    void pause();
    void resume();
    void write(const std::shared_ptr<std::string> &data, double clock);
    // 0 - 100
    void setVolume(int vol);

public slots:
    // 随线程外部绑定，或者直接调用
    void slotStart();
    void slotStop();
    // 内部绑定
    void slotPause();
    void slotResume();
    void slotWrite(const std::shared_ptr<std::string> &data, double clock);
    void slogSetVolume(int vol);

    void handleStateChanged(QAudio::State newState);

private:
    QAudioDevice device;
    QAudioFormat format;
    long long bufferSize;

    double queuedClock;

    QScopedPointer<QAudioSink> audioSink;
    QIODevice *audioDevice = nullptr;
};

class AudioSpeaker : public QObject {
    Q_OBJECT
public:
    explicit AudioSpeaker(bool newThread = true);
    ~AudioSpeaker();

    static QAudioDevice getDevice(const QString &desc = QString());

    // 开始播放
    void start(QAudioDevice dev, QAudioFormat fmt);
    // 停止播放
    void stop();
    // 暂停
    void pause();
    // 恢复
    void resume();

    // 写入音频数据
    void write(const std::shared_ptr<std::string> &data, double clock);
    // 当前可用缓存空间
    int bytesFree();

    // 获取音频当前的时间点，double秒数
    double audioClock();

    // 设置音量[0, 100]
    void setVolume(int vol);

private:
    bool createThread;
    QThread *workThread;
    AudioSpeakerImpl *impl;
    std::atomic_bool sendingData;
};

#endif // AUDIOSPEAKER_H
