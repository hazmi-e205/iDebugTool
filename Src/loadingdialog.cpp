#include "loadingdialog.h"
#include "ui_loadingdialog.h"
#include <QTimer>

LoadingDialog::LoadingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadingDialog)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    connect(ui->done_btn, SIGNAL(pressed()), this, SLOT(close()));
}

LoadingDialog::~LoadingDialog()
{
    delete ui;
}

void LoadingDialog::ShowProgress(QString title, bool withClose)
{
    ui->log_txt->setReadOnly(true);
    ui->log_txt->setText(title);
    setWindowTitle(title);
    setWindowFlags(Qt::Window
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | (withClose ? Qt::WindowCloseButtonHint : Qt::Window)
                   );
    ui->done_btn->setEnabled(false);
    show();

}

void LoadingDialog::SetProgress(int percentage, QString text)
{
    ui->progress_bar->setValue(percentage);
    if (m_lastText != text)
    {
        ui->status_txt->setText(text);
        ui->log_txt->append(text);
        m_lastText = text;
    }
    if (percentage >= 100)
    {
        ui->done_btn->setEnabled(true);
        if (!(ui->log_txt->toPlainText().contains("failed", Qt::CaseInsensitive) || ui->log_txt->toPlainText().contains("error", Qt::CaseInsensitive)))
            QTimer::singleShot(1000, this, &QWidget::close);
    }
}
