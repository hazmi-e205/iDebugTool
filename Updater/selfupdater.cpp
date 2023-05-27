#include "selfupdater.h"
#include "ui_selfupdater.h"

SelfUpdater::SelfUpdater(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SelfUpdater)
{
    ui->setupUi(this);
}

SelfUpdater::~SelfUpdater()
{
    delete ui;
}

