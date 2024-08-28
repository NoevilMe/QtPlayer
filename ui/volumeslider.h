#ifndef XSLIDER_H
#define XSLIDER_H

#include <QSlider>
#include <QWidget>


class VolumeSlider : public QSlider {
    Q_OBJECT
public:
    VolumeSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~VolumeSlider() override;

protected:
    void focusOutEvent(QFocusEvent *ev) override;
};

#endif // XSLIDER_H
