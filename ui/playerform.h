#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

#include "audiospeaker.h"
#include "av_def.h"
#include "player/ffplayer.h"
#include "util/iconhelper.h"

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
    void playAudio(char *buf, int size, double clock);
    void playVideo(AVFrame *frame);

signals:
    void playDoneSignal();

private slots:
    void on_pushButtonList_toggled(bool checked);
    void on_pushButtonFullScreen_toggled(bool checked);

    void playDoneSlot();
    void timerTimeoutSlot();

    // QWidget interface
    void on_pushButtonPlay_clicked(bool checked);

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

private:
    Ui::PlayerForm *ui;

    QScopedPointer<IconHelper> iconHelper;

    // fullscreen
    QWidget *fsWidget_ = nullptr;
    QWidget *fsParent_ = nullptr;
    Qt::WindowFlags fsFlags_;

    QTimer *timerProgress; // 定时器-获取当前视频时间

    std::unique_ptr<AudioSpeaker> speaker;

    std::unique_ptr<FFPlayer> player;
};
#endif // MAINWINDOW_H
