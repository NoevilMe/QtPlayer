#include "volumeslider.h"
#include <QDebug>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

VolumeSlider::VolumeSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent) {

    // this->setFocusPolicy(Qt::StrongFocus);
    this->setStyleSheet(
        R"(QSlider{
background-color: rgba(0, 43, 54, 0.8);
padding-top: 5px;  /*上面端点离顶部的距离*/
padding-bottom: 5px;
border-radius: 4px; /*外边框矩形倒角*/
}
QSlider::groove:vertical {
background: #cbcbcb;
width: 6px;
border-radius: 1px;
padding-left:-1px;
padding-right:-1px;
padding-top:-1px;
padding-bottom:-1px;
}
QSlider::sub-page:vertical {
background: #cbcbcb;
border-radius: 2px;
}
QSlider::add-page:vertical {
background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
    stop:0 #439cf4, stop:1 #439cf4);
background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,
    stop: 0 #439cf4, stop: 1 #439cf4);
width: 10px;
border-radius: 2px;
}
QSlider::handle:vertical
{
width: 10px;
height: 10px;
border-radius:5px;
background:rgb(45,152,255);
margin: 0 -3px;
}
)");
}

VolumeSlider::~VolumeSlider() {}

void VolumeSlider::focusOutEvent(QFocusEvent *ev) {
    qDebug() << "focusOutEvent";
    this->setVisible(false);
    QSlider::focusOutEvent(ev);
}
