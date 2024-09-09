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
    explicit AudioSpeaker(bool createThread = true);
    ~AudioSpeaker();

    static QAudioDevice getDevice(const QString &desc = QString());

    // 获取音频当前的时间点，double秒数
    double audioClock();

    // 开始播放
    void start(QAudioDevice dev, QAudioFormat fmt);
    // 停止播放
    void stop();
    // 暂停
    void pause();
    // 当前可用缓存空间
    int bytesFree();
    // 写入数据
    void write(const std::shared_ptr<std::string> &data, double clock);

signals:
    // 恢复
    void resume();
    // 设置音量[0, 100]
    void setVolume(int vol);

    // 转换调用
    // 写入音频数据
    void writeSink(const std::shared_ptr<std::string> &data, double clock);
    // 暂停
    void pauseSink();

public slots:
    // 随线程外部绑定，或者直接调用
    void slotStartSink();
    void slotStopSink();
    // 内部绑定
    void slotPauseSink();
    void slotResumeSink();
    void slotWriteSink(const std::shared_ptr<std::string> &data, double clock);
    void slotSetVolume(int vol);

    void handleStateChanged(QAudio::State newState);

private:
    bool createThread;
    QThread *oldThread;
    QThread *newThread;
    std::atomic_bool sendingData;

    QAudioDevice device;
    QAudioFormat format;
    long long bufferSize;

    double queuedClock;

    QScopedPointer<QAudioSink> audioSink;
    QIODevice *audioDevice = nullptr;
};

#endif // AUDIOSPEAKER_H
