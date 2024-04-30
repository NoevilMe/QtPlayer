#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

#include "player/ffplayer.h"
#include "av_def.h"

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

private slots:
    void on_pushButtonList_toggled(bool checked);
    void on_pushButtonFullScreen_toggled(bool checked);

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void clickPushButtonFullScreen();

private:
    Ui::PlayerForm *ui;

    // fullscreen
    QWidget *fsWidget_ = nullptr;
    QWidget *fsParent_ = nullptr;
    Qt::WindowFlags fsFlags_;

    std::unique_ptr<FFPlayer> player_;
};
#endif // MAINWINDOW_H
