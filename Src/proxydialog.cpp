#include "proxydialog.h"
#include "ui_proxydialog.h"
#include "userconfigs.h"
#include "simplerequest.h"
#include <QNetworkProxy>
#include <QMessageBox>

ProxyDialog::ProxyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProxyDialog)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    connect(ui->checkBtn, SIGNAL(pressed()), this, SLOT(OnCheckClicked()));
}

ProxyDialog::~ProxyDialog()
{
    delete ui;
}

void ProxyDialog::ShowDialog()
{
    ui->proxyEdit->setText(UserConfigs::Get()->GetData("ProxyServer", ""));
    ui->portEdit->setText(UserConfigs::Get()->GetData("ProxyPort", ""));
    ui->userEdit->setText(UserConfigs::Get()->GetData("ProxyUser", ""));
    ui->passEdit->setText(UserConfigs::Get()->GetData("ProxyPass", ""));
    show();
}

void ProxyDialog::UseExisting()
{
    QString server = UserConfigs::Get()->GetData("ProxyServer", "");
    QString port = UserConfigs::Get()->GetData("ProxyPort", "");
    QString user = UserConfigs::Get()->GetData("ProxyUser", "");
    QString pass = UserConfigs::Get()->GetData("ProxyPass", "");

    QNetworkProxy proxy;
    if (server.isEmpty())
    {
        proxy.setType(QNetworkProxy::NoProxy);
    }
    else
    {
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(server);
        proxy.setPort(port.toUInt());
        proxy.setUser(user);
        proxy.setPassword(pass);
    }
    QNetworkProxy::setApplicationProxy(proxy);
}

void ProxyDialog::OnCheckClicked()
{
    QString server = ui->proxyEdit->text();
    QString port = ui->portEdit->text();
    QString user = ui->userEdit->text();
    QString pass = ui->passEdit->text();
    UserConfigs::Get()->SaveData("ProxyServer", server);
    UserConfigs::Get()->SaveData("ProxyPort", port);
    UserConfigs::Get()->SaveData("ProxyUser", user);
    UserConfigs::Get()->SaveData("ProxyPass", pass);

    QNetworkProxy proxy;
    if (server.isEmpty())
    {
        proxy.setType(QNetworkProxy::NoProxy);
    }
    else
    {
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(server);
        proxy.setPort(port.toUInt());
        proxy.setUser(user);
        proxy.setPassword(pass);
    }
    QNetworkProxy::setApplicationProxy(proxy);

    if (SimpleRequest::IsInternetOn())
        QMessageBox::information(this, "Info", "Internet is available", QMessageBox::Ok);
    else
        QMessageBox::critical(this, "Error", "ERROR: No internet access.", QMessageBox::Ok);
}
