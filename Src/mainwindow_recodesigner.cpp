#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "userconfigs.h"

void MainWindow::SetupRecodesignerUI()
{
    connect(ui->originalBuildBtn, SIGNAL(pressed()), this, SLOT(OnOriginalBuildClicked()));
    connect(ui->privateKeyBtn, SIGNAL(pressed()), this, SLOT(OnPrivateKeyClicked()));
    connect(ui->privateKeyEdit, SIGNAL(currentTextChanged(QString)), this, SLOT(OnPrivateKeyChanged(QString)));
    connect(ui->provisionBtn, SIGNAL(pressed()), this, SLOT(OnProvisionClicked()));
    connect(ui->codesignBtn, SIGNAL(pressed()), this, SLOT(OnCodesignClicked()));
    connect(Recodesigner::Get(), SIGNAL(SigningResult(Recodesigner::SigningStatus,QString)), this, SLOT(OnSigningResult(Recodesigner::SigningStatus,QString)));
    RefreshPrivateKeyList();
}

void MainWindow::RefreshPrivateKeyList()
{
    ui->privateKeyEdit->clear();
    QString privatekeys = UserConfigs::Get()->GetData("PrivateKeys", "");
    QStringList privatekeyslist = privatekeys.split("|");
    foreach (QString privatekey, privatekeyslist) {
        QStringList keypass = privatekeys.split("*");
        ui->privateKeyEdit->addItem(keypass.at(0));
    }

    QString provisionsData = UserConfigs::Get()->GetData("Provisions", "");
    QStringList provisions = provisionsData.split("|");
    if (ui->provisionEdit->count() == 0 && provisions.count() > 0)
        ui->provisionEdit->addItems(provisions);
}

void MainWindow::OnOriginalBuildClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "OriginalBuild", this);
    ui->originalBuildEdit->setText(filepath);
}

void MainWindow::OnPrivateKeyClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "PrivateKey", this);
    ui->privateKeyEdit->setEditText(filepath);
}

void MainWindow::OnProvisionClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "Provision", this);
    ui->provisionEdit->setEditText(filepath);
}

void MainWindow::OnCodesignClicked()
{
    Recodesigner::Params params;
    params.PrivateKey = ui->privateKeyEdit->currentText();
    params.PrivateKeyPassword = ui->privateKeyPasswordEdit->text();
    params.Provision = ui->provisionEdit->currentText();
    params.OriginalBuild = ui->originalBuildEdit->text();
    params.DoUnpack = ui->UnpackCheck->isChecked();
    params.DoCodesign = ui->CodesignCheck->isChecked();
    params.DoRepack = ui->RepackCheck->isChecked();
    params.DoInstall = ui->InstallCheck->isChecked();
    Recodesigner::Get()->Process(params);

    QString privatekeys = UserConfigs::Get()->GetData("PrivateKeys", "");
    if (privatekeys.toLower().contains(params.PrivateKey.toLower()))
    {
        QStringList privatekeylist = privatekeys.split("|");
        privatekeys = "";
        foreach (QString privatekey, privatekeylist) {
            QStringList keypass = privatekey.split("*");
            privatekeys = (privatekeys.isEmpty() ? "" : (privatekeys + "|"))
                    + keypass.at(0) + "*"
                    + (keypass.at(0).toLower() == params.PrivateKey.toLower() ? params.PrivateKeyPassword : keypass.at(1));
        }
        UserConfigs::Get()->SaveData("PrivateKeys", privatekeys);
    }
    else
    {
        UserConfigs::Get()->SaveData("PrivateKeys", (privatekeys.isEmpty() ? "" : (privatekeys + "|")) + params.PrivateKey + "*" + params.PrivateKeyPassword);
    }

    QString provisionsData = UserConfigs::Get()->GetData("Provisions", "");
    QStringList provisions = provisionsData.split("|");
    if (!provisions.contains(params.Provision) && !params.Provision.isEmpty())
    {
        UserConfigs::Get()->SaveData("Provisions", (provisionsData.isEmpty() ? "" : (provisionsData + "|")) + params.Provision);
        ui->provisionEdit->addItem(params.Provision);
    }
    RefreshPrivateKeyList();
    ui->codesignBtn->setEnabled(false);
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnPrivateKeyChanged(QString key)
{
    QString privatekeys = UserConfigs::Get()->GetData("PrivateKeys", "");
    QStringList privatekeyslist = privatekeys.split("|");
    foreach (QString privatekey, privatekeyslist) {
        QStringList keypass = privatekey.split("*");
        if (keypass.at(0).toLower() == key.toLower())
        {
            ui->privateKeyPasswordEdit->setText(keypass.at(1));
            break;
        }
    }
}

void MainWindow::OnSigningResult(Recodesigner::SigningStatus status, QString messages)
{
    ui->outputEdit->appendPlainText(messages);
    if (status == Recodesigner::SigningStatus::FAILED || status == Recodesigner::SigningStatus::SUCCESS || status == Recodesigner::SigningStatus::INSTALL)
    {
        ui->codesignBtn->setEnabled(true);
        if (status == Recodesigner::SigningStatus::INSTALL)
        {
            QStringList buildFound = FindDirs(GetDirectory(DIRECTORY_TYPE::ZSIGN_TEMP), QStringList() << "*.app");
            if (buildFound.count() == 0)
                FindFiles(GetDirectory(DIRECTORY_TYPE::RECODESIGNED), QStringList() << "*.ipa");

            if (buildFound.count() > 0)
            {
                ui->featureWidget->setCurrentIndex(0);
                ui->installPath->setText(buildFound.at(0));
                ui->installBtn->click();
            }
        }
    }
}
