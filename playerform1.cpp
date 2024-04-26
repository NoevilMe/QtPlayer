#include "playerform.h"
#include "ui_playerform.h"

PlayerForm::PlayerForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayerForm)
{
    ui->setupUi(this);
}

PlayerForm::~PlayerForm()
{
    delete ui;
}
