#include "playerform.h"
#include "ui_playerform.h"

#include "player/device.h"

#include <QFile>
#include <QStyle>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>



PlayerForm::PlayerForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::PlayerForm) {
    // installWindowAgent();

    ui->setupUi(this);
    ui->listWidgetFiles->hide();

    // ui->openGLWidget->init(1280, 720);



    auto devices = getVideoDevices();
    for (auto &d : devices) {
        qDebug() << "device " << d.name;
    }

    player_.reset(new FFPlayer);
    player_->SetFrameCallback(std::bind(&YuvVideoWidget::paintAVFrame,
                                        ui->openGLWidget,
                                        std::placeholders::_1));
    player_->Start();
}

PlayerForm::~PlayerForm() { delete ui;
    qDebug()<<"PlayerForm::~PlayerForm() ";

    if (player_) {
        player_->Stop();
    }
}


void PlayerForm::clickPushButtonFullScreen() {
    //对pushButton实现模拟点击
    //定义左键点击事件，Qt::NoModifier代表无其他修饰键被按下
    // QMouseEvent mouseDown(QEvent::MouseButtonPress, QPoint(1, 1),
    //                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    // //定义左键释放事件，Qt::NoModifier代表无其他修饰键被按下
    // QMouseEvent mouseUp(QEvent::MouseButtonRelease, QPoint(1, 1),
    //                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    // //向按钮pushButton发送鼠标左键按下事件，之后发送鼠标左键释放事件，模拟一次点击
    // QApplication::sendEvent(ui->pushButtonFullScreen, &mouseDown);
    // QApplication::sendEvent(ui->pushButtonFullScreen, &mouseUp);
}

// bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
//    if (fsWidget_ != nullptr && watched == fsWidget_ &&
//        event->type() == QEvent::KeyPress) {
//        qDebug() << "ESC";
//        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//        if (keyEvent->key() == Qt::Key_Escape) {
//            clickPushButtonFullScreen();

//            return true; // 事件已处理，不传递给其他对象
//        }
//    }
//    return QMainWindow::eventFilter(watched, event); // 将事件传递给基类处理
//}

void PlayerForm::keyPressEvent(QKeyEvent *event) {
    // if (this->isFullScreen() && event->key() == Qt::Key_Escape) {
    //     qDebug() << "ESC";
    //     clickPushButtonFullScreen();
    // }
}

void PlayerForm::on_pushButtonList_toggled(bool checked) {
    if (ui->listWidgetFiles->isHidden()) {
        ui->listWidgetFiles->show();
    } else {
        ui->listWidgetFiles->hide();
    }
}

void PlayerForm::on_pushButtonFullScreen_toggled(bool checked) {
    // https://blog.csdn.net/gdizcm/article/details/131649492
    // https://blog.csdn.net/bai2010bingbing/article/details/91378903
    // https://www.cnblogs.com/wuhanpjf/p/11247770.html
    // https://www.cnblogs.com/lvdongjie/p/3758025.html

    // if (checked) {
    //     qDebug() << "enable full screen";


    //     this->menuWidget()->hide();
    //     ui->widgetControl->hide();
    //     ui->listWidgetFiles->hide();
    //     this->showFullScreen();
    //     //        this->hide();

    // } else {
    //     qDebug() << "disable full screen";
    //     this->menuWidget()->show();
    //     ui->widgetControl->show();
    //     this->showNormal();
    //     //        this->show();
    // }

}


void PlayerForm::closeEvent(QCloseEvent *event)
{
    qDebug()<<"PlayerForm::closeEvent";
}
