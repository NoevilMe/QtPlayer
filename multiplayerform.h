#ifndef MULTIPLAYERFORM_H
#define MULTIPLAYERFORM_H

#include <QWidget>

namespace Ui {
class MultiPlayerForm;
}

class MultiPlayerForm : public QWidget
{
    Q_OBJECT

public:
    explicit MultiPlayerForm(QWidget *parent = nullptr);
    ~MultiPlayerForm();

private:
    Ui::MultiPlayerForm *ui;
};

#endif // MULTIPLAYERFORM_H
