#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QJsonObject>
#include <QMessageBox>

void MainWindow::SetupAppManagerUI()
{
    setAcceptDrops(true);
    ui->installBar->setAlignment(Qt::AlignCenter);
    connect(ui->installBtn, SIGNAL(pressed()), this, SLOT(OnInstallClicked()));
    connect(ui->UninstallBtn, SIGNAL(pressed()), this, SLOT(OnUninstallClicked()));
    connect(DeviceBridge::Get(), SIGNAL(InstallerStatusChanged(InstallerMode,QString,int,QString)), this, SLOT(OnInstallerStatusChanged(InstallerMode,QString,int,QString)));
    connect(ui->bundleIds, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));
    connect(ui->appInfoBtn, SIGNAL(pressed()), this, SLOT(OnAppInfoClicked()));
}

void MainWindow::OnInstallClicked()
{
    DeviceBridge::Get()->InstallApp(ui->upgrade->isChecked() ? InstallerMode::CMD_UPGRADE : InstallerMode::CMD_INSTALL, ui->installPath->text());
    ui->installBtn->setEnabled(false);
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnUninstallClicked()
{
    if (!m_choosenBundleId.isEmpty())
    {
        DeviceBridge::Get()->UninstallApp(m_choosenBundleId);
    }
}

void MainWindow::OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message)
{
    switch (command)
    {
    case InstallerMode::CMD_INSTALL:
        {
            QString messages = (percentage >= 0 ? ("(" + QString::number(percentage) + "%) ") : "") + message;
            ui->installBar->setFormat("%p% " + (message.contains('\n') ? message.split('\n').at(0) : message));
            ui->installBar->setValue(percentage);
            ui->outputEdit->appendPlainText(messages);
            if (percentage == 100)
                ui->installBtn->setEnabled(true);
        }
        break;

    case InstallerMode::CMD_UNINSTALL:
        if (percentage == 100)
            QMessageBox::information(this, "Uninstall Success!", bundleId + " uninstalled.", QMessageBox::Ok);
        break;

    default:
        break;
    }
}

void MainWindow::OnBundleIdChanged(QString text)
{
    m_choosenBundleId = text;
    auto app_info = m_installedApps[text];
    ui->AppName->setText(app_info["CFBundleName"].toString());
    ui->AppVersion->setText(app_info["CFBundleShortVersionString"].toString());
    ui->AppSigner->setText(app_info["SignerIdentity"].toString());
}

void MainWindow::OnAppInfoClicked()
{
    if(!m_choosenBundleId.isEmpty())
        m_textDialog->ShowText("App Information",m_installedApps[m_choosenBundleId].toJson());
}

void MainWindow::RefreshPIDandBundleID()
{
    m_installedApps = DeviceBridge::Get()->GetInstalledApps(true);
    ui->bundleIds->clear();
    ui->bundleIds->addItems(m_installedApps.keys());

    ui->storageOption->clear();
    ui->storageOption->addItem("User's Data");
    ui->storageOption->addItems(m_installedApps.keys());

    QString old_string = ui->pidEdit->currentText();
    ui->pidEdit->clear();
    ui->pidEdit->addItems(DeviceBridge::GetPIDOptions(m_installedApps));
    if (!old_string.isEmpty())
        ui->pidEdit->setEditText(old_string);

    old_string = ui->bundleEdit->currentText();
    ui->bundleEdit->clear();
    ui->bundleEdit->addItems(m_installedApps.keys());
    if (!old_string.isEmpty())
        ui->bundleEdit->setEditText(old_string);
}
