#ifndef OPENMEDIADIALOG_H
#define OPENMEDIADIALOG_H

#include "av_def.h"

#include <QDialog>

namespace Ui {
class OpenMediaDialog;
}

class OpenMediaDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenMediaDialog(QWidget *parent = nullptr);
    ~OpenMediaDialog();

    MediaSource mediaSource;

private:
    void initCaptureDevices();

private:
    Ui::OpenMediaDialog *ui;

    // QDialog interface
public slots:
    void accept() override;

private slots:
    void on_pushButtonOpen_clicked();
};

#endif // OPENMEDIADIALOG_H
