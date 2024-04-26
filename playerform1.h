#ifndef PLAYERFORM_H
#define PLAYERFORM_H

#include <QWidget>

namespace Ui {
class PlayerForm;
}

class PlayerForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerForm(QWidget *parent = nullptr);
    ~PlayerForm();

private:
    Ui::PlayerForm *ui;
};

#endif // PLAYERFORM_H
