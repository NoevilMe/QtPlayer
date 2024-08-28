#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

#include "audiospeaker.h"
#include "av_def.h"
#include "player/ffplayer.h"
#include "util/iconhelper.h"
#include "volumeslider.h"

#include <QScopedPointer>

class QIODevice;

QT_BEGIN_NAMESPACE
namespace Ui {
class PlayerForm;
}
QT_END_NAMESPACE

class PlayerForm : public QWidget {
    Q_OBJECT

public:
    PlayerForm(QWidget *parent = nullptr);
    ~PlayerForm();

    bool openMedia(MediaSource media);
    void playAudio(const char *data, int size, double clock);
    void playVideo(AVFrame *frame, double clock);
    bool NegotiateAudioFormat(const AudioFormat *in, AudioFormat *out);

    void setWindowTitleCb(const std::function<void(const QString &)> &cb) {
        windowTitleCb = cb;
    }

    void openDialog();

signals:
    void playDone();

private slots:
    void on_pushButtonList_toggled(bool checked);
    void on_pushButtonFullScreen_toggled(bool checked);

    void slotPlayDone();
    void slotTimerTimeout();
    void slotVolumeChanged(int value);

    // 播放、暂停
    void on_pushButtonPlay_clicked(bool checked);
    // 停止
    void on_pushButtonStop_clicked();

    void on_pushButtonVolume_clicked(bool checked);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void loadIcons();
    void clickPushButtonFullScreen();
    void listOutputAudioDevices();

    void onTotalSeconds(double seconds);

    void stopPlayer();
    void stopSpeaker();
    void pauseSpeaker();
    void resumeSpeaker();

    void initContext();
    void clearContext();

    void setVolumeIcon(bool mute);

    QString formatTimestamp(int seconds, bool longFmt);
    void clearTimestamp();

private:
    Ui::PlayerForm *ui;

    QScopedPointer<IconHelper> iconHelper;

    // fullscreen
    QWidget *fsWidget_ = nullptr;
    QWidget *fsParent_ = nullptr;
    Qt::WindowFlags fsFlags_;

    QTimer *timerProgress; // 定时器-获取当前视频时间

    VolumeSlider *volumeSlider = nullptr;
    int volume = 0;

    std::function<void(const QString &)> windowTitleCb;

    std::unique_ptr<AudioSpeaker> speaker;
    std::unique_ptr<FFPlayer> player;

    // QWidget interface
protected:
    void mousePressEvent(QMouseEvent *event) override;
};
#endif // MAINWINDOW_H
