#include "multiplayerform.h"
#include "ui_multiplayerform.h"

MultiPlayerForm::MultiPlayerForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MultiPlayerForm)
{
    ui->setupUi(this);
}

MultiPlayerForm::~MultiPlayerForm()
{
    delete ui;
}
