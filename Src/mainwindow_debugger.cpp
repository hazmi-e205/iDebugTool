#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::SetupDebuggerUI()
{
    connect(ui->startDebugBtn, SIGNAL(pressed()), this, SLOT(OnStartDebuggingClicked()));
    connect(ui->bundleEdit, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));
}

void MainWindow::OnStartDebuggingClicked()
{
    if (ui->startDebugBtn->text().contains("start", Qt::CaseInsensitive))
    {
        DeviceBridge::Get()->StartDebugging(ui->bundleEdit->currentText(), false, ui->envEdit->text(), ui->argsEdit->text());
        ui->startDebugBtn->setText("Stop Debugging");
    }
    else
    {
        DeviceBridge::Get()->StopDebugging();
        ui->startDebugBtn->setText("Start Debugging");
    }
}
