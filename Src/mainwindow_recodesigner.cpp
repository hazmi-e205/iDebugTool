#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "userconfigs.h"
#include <QJsonObject>

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
    QJsonObject privatekeys = UserConfigs::Get()->GetData("PrivateKey", QJsonObject());
    foreach (const QString& keyname, privatekeys.keys())
        ui->privateKeyEdit->addItem(keyname);

    ui->provisionEdit->clear();
    QStringList provisions = UserConfigs::Get()->GetData("Provision", QStringList());
    foreach (const QString& provname, provisions)
        ui->provisionEdit->addItem(provname);
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
    params.ProvisionExt1 = ui->provisionExt1Edit->currentText();
    params.ProvisionExt2 = ui->provisionExt2Edit->currentText();
    params.NewBundleId = ui->newBundleIdEdit->text();
    params.NewBundleVersion = ui->newBundleVersionEdit->text();
    params.NewDisplayName = ui->newDisplayNameEdit->text();
    params.NewEntitlements = ui->newEntitlementEdit->text();
    params.OriginalBuild = ui->originalBuildEdit->text();
    params.DoUnpack = ui->UnpackCheck->isChecked();
    params.DoCodesign = ui->CodesignCheck->isChecked();
    params.DoRepack = ui->RepackCheck->isChecked();
    params.DoInstall = ui->InstallCheck->isChecked();
    Recodesigner::Get()->Process(params);

    if (!params.PrivateKey.isEmpty())
    {
        QJsonObject privatekeys = UserConfigs::Get()->GetData("PrivateKey", QJsonObject());
        if (!privatekeys.keys().contains(params.PrivateKey, Qt::CaseInsensitive))
            ui->privateKeyEdit->addItem(params.PrivateKey);
        privatekeys[params.PrivateKey] = params.PrivateKeyPassword;
        UserConfigs::Get()->SaveData("PrivateKey", privatekeys);
    }

    if (!params.Provision.isEmpty())
    {
        QStringList provisions = UserConfigs::Get()->GetData("Provision", QStringList());
        if (!provisions.contains(params.Provision, Qt::CaseInsensitive)) {
            ui->provisionEdit->addItem(params.Provision);
            provisions.append(params.Provision);
        }
        UserConfigs::Get()->SaveData("Provision", provisions);
    }

    ui->codesignBtn->setEnabled(false);
    ui->bottomWidget->setCurrentIndex(1);
}

void MainWindow::OnPrivateKeyChanged(QString key)
{
    QJsonObject privatekeys = UserConfigs::Get()->GetData("PrivateKey", QJsonObject());
    if (privatekeys.keys().contains(key, Qt::CaseInsensitive))
        ui->privateKeyPasswordEdit->setText(privatekeys[key].toString());
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
