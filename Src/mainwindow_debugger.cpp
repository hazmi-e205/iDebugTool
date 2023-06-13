#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include <QFile>

void MainWindow::SetupDebuggerUI()
{
    connect(ui->startDebugBtn, SIGNAL(pressed()), this, SLOT(OnStartDebuggingClicked()));
    connect(ui->bundleEdit, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));
    connect(DeviceBridge::Get(), SIGNAL(DebuggerReceived(QString,bool)), this, SLOT(OnDebuggerReceived(QString,bool)));
    connect(DeviceBridge::Get(), SIGNAL(DebuggerFilterStatus(bool)), this, SLOT(OnDebuggerFilterStatus(bool)));
    connect(ui->searchDbgEdit, SIGNAL(textChanged(QString)), this, SLOT(OnDebuggerFilterChanged(QString)));
    connect(ui->excludeDbgEdit, SIGNAL(textChanged(QString)), this, SLOT(OnDebuggerExcludeChanged(QString)));
    connect(ui->clearDebugBtn, SIGNAL(pressed()), this, SLOT(OnDebuggerClearClicked()));
    connect(ui->saveDebugBtn, SIGNAL(pressed()), this, SLOT(OnDebuggerSaveClicked()));

    DeviceBridge::Get()->DebuggerFilterByString(ui->searchDbgEdit->text());
    DeviceBridge::Get()->DebuggerExcludeByString(ui->excludeDbgEdit->text());
    DeviceBridge::Get()->SetMaxDebuggerLogs(m_maxCachedLogs);
    ui->maxShownLogs->setText(QString::number(m_maxCachedLogs));
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
    }
}

void MainWindow::OnDebuggerClearClicked()
{
    ui->debuggerEdit->clear();
    DeviceBridge::Get()->ClearCachedLogs();
}

void MainWindow::OnDebuggerSaveClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Log", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << ui->debuggerEdit->toPlainText();
            f.close();
        }
    }
}

void MainWindow::OnDebuggerReceived(QString logs, bool stopped)
{
    ui->debuggerEdit->appendPlainText(logs);
    if (stopped)
        ui->startDebugBtn->setText("Start Debugging");
}

void MainWindow::OnDebuggerFilterStatus(bool isfiltering)
{
    if (isfiltering)
        ui->debuggerEdit->setPlainText(QString("Filterring about %1 cached logs...").arg(m_maxCachedLogs));
    else
        ui->debuggerEdit->clear();
}

void MainWindow::OnDebuggerFilterChanged(QString text)
{
    ui->debuggerEdit->clear();
    DeviceBridge::Get()->DebuggerFilterByString(text);
}

void MainWindow::OnDebuggerExcludeChanged(QString text)
{
    ui->debuggerEdit->clear();
    DeviceBridge::Get()->DebuggerExcludeByString(text);
}
