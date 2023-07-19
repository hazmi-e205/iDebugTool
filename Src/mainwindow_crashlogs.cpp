#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "crashsymbolicator.h"
#include <QMessageBox>
#include <QDesktopServices>

void MainWindow::SetupCrashlogsUI()
{
    connect(ui->syncCrashlogsBtn, SIGNAL(pressed()), this, SLOT(OnSyncCrashlogsClicked()));
    connect(DeviceBridge::Get(), SIGNAL(CrashlogsStatusChanged(QString)), this, SLOT(OnCrashlogsStatusChanged(QString)));
    connect(ui->crashlogBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogClicked()));
    connect(ui->crashlogsDirBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogsDirClicked()));
    connect(ui->dsymBtn, SIGNAL(pressed()), this, SLOT(OnDsymClicked()));
    connect(ui->dwarfBtn, SIGNAL(pressed()), this, SLOT(OnDwarfClicked()));
    connect(ui->symbolicateBtn, SIGNAL(pressed()), this, SLOT(OnSymbolicateClicked()));
    connect(CrashSymbolicator::Get(), SIGNAL(SymbolicateResult(QString,bool)), this, SLOT(OnSymbolicateResult(QString,bool)));
}

void MainWindow::OnSyncCrashlogsClicked()
{
    DeviceBridge::Get()->SyncCrashlogs(GetDirectory(DIRECTORY_TYPE::CRASHLOGS));
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnCrashlogsDirClicked()
{
    if (!QDesktopServices::openUrl(GetDirectory(DIRECTORY_TYPE::CRASHLOGS)))
    {
        ui->bottomWidget->setCurrentIndex(1);
        ui->outputEdit->appendPlainText("No synchronized crashlogs from device.");
    }
}

void MainWindow::OnCrashlogsStatusChanged(QString text)
{
    ui->outputEdit->appendPlainText(text);
}

void MainWindow::OnCrashlogClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "Crashlog", this);
    ui->crashlogEdit->setText(filepath);
}

void MainWindow::OnDsymClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_DIR, "dSYM", this);
    ui->dsymEdit->setText(filepath);
}

void MainWindow::OnDwarfClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "DWARF", this);
    ui->dsymEdit->setText(filepath);
}

void MainWindow::OnSymbolicateClicked()
{
    QString crashpath = ui->crashlogEdit->text();
    QString dsympath = ui->dsymEdit->text();
    CrashSymbolicator::Get()->Process(crashpath, dsympath);
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnSymbolicateResult(QString messages, bool error)
{
    if (error)
    {
        QMessageBox::critical(this, "Error", messages, QMessageBox::Ok);
        ui->statusbar->showMessage("Symbolication failed!");
        ui->outputEdit->appendPlainText(messages);
    }
    else
    {
        QMessageBox msgBox(QMessageBox::Question, "Symbolication", "Symbolication success and saved to\n" + messages);
        QPushButton *openButton = msgBox.addButton("Open it!", QMessageBox::ButtonRole::ActionRole);
        QPushButton *dirButton = msgBox.addButton("Go to directory...", QMessageBox::ButtonRole::ActionRole);
        msgBox.addButton(QMessageBox::StandardButton::Close);
        msgBox.exec();
        if (msgBox.clickedButton() == openButton)
        {
            QDesktopServices::openUrl(messages);
        }
        else if (msgBox.clickedButton() == dirButton)
        {
            QDesktopServices::openUrl(GetDirectory(DIRECTORY_TYPE::SYMBOLICATED));
        }
        ui->outputEdit->appendPlainText("Symbolicated file saved to '" + messages + "'!");
        ui->statusbar->showMessage("Symbolication success!");
    }
}
