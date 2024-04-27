#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

#include "player/ffplayer.h"

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


private slots:
    void on_pushButtonList_toggled(bool checked);
    void on_pushButtonFullScreen_toggled(bool checked);


private:
    Ui::PlayerForm *ui;

    // fullscreen
    QWidget *fsWidget_ = nullptr;
    QWidget *fsParent_ = nullptr;
    Qt::WindowFlags fsFlags_;


    void clickPushButtonFullScreen();

    // QObject interface
    // public:
    //    bool eventFilter(QObject *watched, QEvent *event) override;


    std::unique_ptr<FFPlayer> player_;

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent *event) override;

    // QWidget interface
protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
