#include "progressslider.h"

#include <QMouseEvent>
#include <QStyle>

ProgressSlider::ProgressSlider(QWidget *parent) : QSlider(parent) {
    setTracking(false);
    setPageStep(0);
}

ProgressSlider::ProgressSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent) {
    setTracking(false);
    setPageStep(0);
}

ProgressSlider::~ProgressSlider() {}

void ProgressSlider::mousePressEvent(QMouseEvent *event) {

    // {
    //     // 获取点击触发前的值
    //     const int value = this->value();
    //     // 调用父类的鼠标点击处理事件
    //     QSlider::mousePressEvent(event);
    //     setValue(value);
    //     return;
    // }

    /*
    //获取当前点击位置,得到的这个鼠标坐标是相对于当前QSlider的坐标
    int currentX = event->pos().x();
    //获取当前点击的位置占整个Slider的百分比
    double per = currentX * 1.0 / this->width();
    //利用算得的百分比得到具体数字
    // int value = per * (this->maximum() - this->minimum()) +
    this->minimum();*/

    // 注意应先调用父类的鼠标点击处理事件，这样可以不影响拖动的情况
    // 滑动条移动事件等事件也用到了mousePressEvent,加这句话是为了不对其产生影响，是的Slider能正常相应其他鼠标事件
    QSlider::mousePressEvent(event);

    if (!this->isSliderDown()) {
        int value = QStyle::sliderValueFromPosition(minimum(), maximum(),
                                                    event->pos().x(), width());
        // 设定滑动条位置
        this->setValue(value);
        qDebug() << "mousePressEvent emit sliderChanged" << value;
        emit sliderChanged(value);
    }
}
