#ifndef PROGRESSSLIDER_H
#define PROGRESSSLIDER_H

#include <QSlider>

class ProgressSlider : public QSlider {
    Q_OBJECT
public:
    ProgressSlider(QWidget *parent = nullptr);
    ProgressSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    virtual ~ProgressSlider() override;

signals:
    void sliderChanged(int value);

    // QWidget interface
protected:
    void mousePressEvent(QMouseEvent *event) override;
};

#endif // PROGRESSSLIDER_H
