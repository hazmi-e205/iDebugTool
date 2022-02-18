#include "textviewer.h"
#include "ui_textviewer.h"

TextViewer::TextViewer(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextViewer)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(pressed()), this, SLOT(close()));
}

TextViewer::~TextViewer()
{
    delete ui;
}

void TextViewer::ShowText(QString title, QString text)
{
    ui->textEdit->setText(text);
    this->setWindowTitle(title);
    this->show();
}
