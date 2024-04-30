#include "openmediadialog.h"
#include "ui_openmediadialog.h"

#include "player/device.h"

#include <QFileDialog>

OpenMediaDialog::OpenMediaDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::OpenMediaDialog) {
    ui->setupUi(this);

    setFixedSize(600, 300);

    initCaptureDevices();
}

OpenMediaDialog::~OpenMediaDialog() { delete ui; }

void OpenMediaDialog::initCaptureDevices()
{
    auto devices = getVideoDevices();
    for(auto &d: devices) {
        ui->comboBoxVideo->addItem(d.name);
    }
}

void OpenMediaDialog::accept() {
    if (ui->tabWidgetOpenMedia->currentWidget() == ui->tabFile) {
        mediaSource.type = MediaType::kMediaFile;
        mediaSource.src = ui->lineEditFile->text().toStdString();

    } else if (ui->tabWidgetOpenMedia->currentWidget() == ui->tabNetwork) {
        mediaSource.type = MediaType::kMediaNetwork;
        mediaSource.src = ui->lineEditNetwork->text().toStdString();
    } else if (ui->tabWidgetOpenMedia->currentWidget() == ui->tabCapture) {
        mediaSource.type = MediaType::kMediaCapture;
        mediaSource.src = ui->comboBoxVideo->currentText().toStdString();
    } else {
        mediaSource.type = MediaType::kMediaNone;
    }

    QDialog::accept();
}

void OpenMediaDialog::on_pushButtonOpen_clicked()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open Meida File"), "",
                                                "Video Files(*.3gp *.amv *.asf *.avi *.flv *.m2v *.m4v *.mkv *.mp2 *.mp4 *.mpg *.swf *.ts *.rmvb *.wmv)\n"
                                                "All Files(*)");
    if (!file.isEmpty()) {
        ui->lineEditFile->setText(file);
    }
}

