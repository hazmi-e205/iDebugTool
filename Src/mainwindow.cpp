#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "appinfo.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_appInfo = new AppInfo(this);
    QMainWindow::setWindowTitle(m_appInfo->GetFullname());
    QMainWindow::setWindowIcon(QIcon(":res/bulb.ico"));
    DeviceBridge::Get()->Init(this);
    connect(DeviceBridge::Get(), SIGNAL(UpdateDevices(std::vector<Device>)),this,SLOT(OnUpdateDevices(std::vector<Device>)));
    connect(DeviceBridge::Get(), SIGNAL(DeviceInfo(QJsonDocument)),this,SLOT(OnDeviceInfo(QJsonDocument)));
}

MainWindow::~MainWindow()
{
    DeviceBridge::Destroy();
    delete m_appInfo;
    delete ui;
}

void MainWindow::OnUpdateDevices(std::vector<Device> devices)
{
    if(devices.size() > 0)
        DeviceBridge::Get()->ConnectToDevice(devices[0]);
}

void MainWindow::OnDeviceInfo(QJsonDocument info)
{
    qDebug() << info["DeviceName"].toString();
}
