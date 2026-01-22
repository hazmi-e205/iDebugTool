#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "userconfigs.h"
#include "utils.h"
#include "usbmuxd.h"
#include <QMessageBox>

void MainWindow::SetupDevicesUI()
{
    if (!m_devicesModel) {
        m_devicesModel = new QStandardItemModel();
        ui->deviceTable->setModel(m_devicesModel);
        ui->deviceTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ui->deviceTable->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        ui->deviceTable->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
        m_loadingDevice->close();

        connect(ui->deviceTable, SIGNAL(clicked(QModelIndex)), this, SLOT(OnDevicesTableClicked(QModelIndex)));
        connect(ui->refreshBtn, SIGNAL(pressed()), this, SLOT(OnRefreshClicked()));
        connect(ui->socketBtn, SIGNAL(pressed()), this, SLOT(OnSocketClicked()));
        connect(ui->sysInfoBtn, SIGNAL(pressed()), this, SLOT(OnSystemInfoClicked()));
        connect(DeviceBridge::Get(), SIGNAL(UpdateDevices(QMap<QString,idevice_connection_type>)), this, SLOT(OnUpdateDevices(QMap<QString,idevice_connection_type>)));
        connect(DeviceBridge::Get(), SIGNAL(DeviceStatus(ConnectionStatus, QString, bool)), this, SLOT(OnDeviceStatus(ConnectionStatus, QString, bool)));
        connect(DeviceBridge::Get(), SIGNAL(ProcessStatusChanged(int,QString)), this, SLOT(OnProcessStatusChanged(int,QString)));
        RefreshSocketList();
    }
    m_devicesModel->setHorizontalHeaderItem(0, new QStandardItem("UDID"));
    m_devicesModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_devicesModel->setHorizontalHeaderItem(2, new QStandardItem("Connection"));
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
}

void MainWindow::OnSystemInfoClicked()
{
    if(DeviceBridge::Get()->IsConnected())
        m_textDialog->ShowText("System Information", DeviceBridge::Get()->GetDeviceInfo().toJson());
}

void MainWindow::UpdateInfoWidget()
{
    auto deviceinfo = DeviceBridge::Get()->GetDeviceInfo();
    ui->ProductType->setText(deviceinfo["ProductType"].toString());
    ui->OSName->setText(deviceinfo["ProductName"].toString());
    ui->OSVersion->setText(deviceinfo["ProductVersion"].toString());
    ui->CPUArch->setText(deviceinfo["CPUArchitecture"].toString());
    ui->UDID->setText(deviceinfo["UniqueDeviceID"].toString());
}

void MainWindow::OnDevicesTableClicked(QModelIndex selectedIndex)
{
    if (!selectedIndex.isValid())
        return;

    QModelIndexList indexes = ui->deviceTable->selectionModel()->selection().indexes();
    QString choosenUdid;
    for (int i = 0; i < indexes.count(); ++i)
    {
        QModelIndex index = indexes.at(i);
        if (index.column() == 0)
        {
            choosenUdid = index.model()->index(index.row(),index.column()).data().toString();
        }
    }
    DeviceBridge::Get()->ConnectToDevice(choosenUdid);
    UpdateInfoWidget();
}

void MainWindow::OnRefreshClicked()
{
    OnUpdateDevices(DeviceBridge::Get()->GetDevices());
}

void MainWindow::OnUpdateDevices(QMap<QString, idevice_connection_type> devices)
{
    m_devicesModel->clear();
    SetupDevicesUI();

    foreach (const auto& udid, devices.keys())
    {
        QList<QStandardItem*> rowData;
        QString name = DeviceBridge::Get()->GetDeviceInfo(udid)["DeviceName"].toString();
        rowData << new QStandardItem(udid);
        rowData << new QStandardItem(name);
        rowData << new QStandardItem(devices[udid] == idevice_connection_type::CONNECTION_NETWORK ? "network" : "usbmuxd");
        m_devicesModel->appendRow(rowData);
    }
}

void MainWindow::OnDeviceStatus(ConnectionStatus status, QString udid, bool isRemote)
{
    if (status == ConnectionStatus::CONNECTED)
    {
        UpdateInfoWidget();

        // update device name on choosen device
        for (int idx = 0; idx < m_devicesModel->rowCount(); idx++)
        {
            QString currentUdid = m_devicesModel->data(m_devicesModel->index(idx, 0)).toString();
            if (currentUdid.contains(udid))
            {
                QString name = DeviceBridge::Get()->GetDeviceInfo()["DeviceName"].toString();
                m_devicesModel->setData(m_devicesModel->index(idx, 1), name);
            }
        }

        RefreshPIDandBundleID(false);
        OnPidFilterChanged(ui->pidEdit->currentText());
        if (isRemote)
            ui->socketBtn->setText("Disconnect");
    }
    else
    {
        ui->ProductType->clear();
        ui->OSName->clear();
        ui->OSVersion->clear();
        ui->CPUArch->clear();
        ui->UDID->clear();
        ResetFileBrowser();
        if (isRemote)
            ui->socketBtn->setText("Connect");
    }
}

void MainWindow::OnSocketClicked()
{
    if (ui->socketBtn->text() == "Connect")
    {
        QString text = FindRegex(ui->socketBox->currentText(), "(\\S+):(\\d+)");
        QStringList text_split = text.split(":");
        if (text_split.length() == 2)
        {
            if (ui->remoteType->currentText().contains("sonic", Qt::CaseInsensitive))
            {
                QString historyData = UserConfigs::Get()->GetData("SocketHistory", "");
                if (!historyData.contains(text))
                {
                    historyData.append(text + ";");
                    UserConfigs::Get()->SaveData("SocketHistory", historyData);
                }
                DeviceBridge::Get()->ConnectToDevice(text_split[0], text_split[1].toUInt());
            }
            else if (ui->remoteType->currentText().contains("stf", Qt::CaseInsensitive) && usbmuxd_connect_remote(text_split[0].toUtf8().data(), text_split[1].toUInt()) >= 0)
            {
                ui->socketBtn->setText("Disconnect");
                QString historyData = UserConfigs::Get()->GetData("SocketHistory", "");
                if (!historyData.contains(text))
                {
                    historyData.append(text + ";");
                    UserConfigs::Get()->SaveData("SocketHistory", historyData);
                    RefreshSocketList();
                }
            }
            else
            {
                QMessageBox::critical(this, "Error", "Error: fail to connect '" + text + "' device via socket", QMessageBox::Ok);
            }
        }
    }
    else
    {
        DeviceBridge::Get()->ResetConnection();
        if (ui->remoteType->currentText().contains("stf", Qt::CaseInsensitive))
            usbmuxd_disconnect_remote();
        ui->socketBtn->setText("Connect");
    }

    //update devce list
    OnUpdateDevices(DeviceBridge::Get()->GetDevices());
}

void MainWindow::RefreshSocketList()
{
    QString historyData = UserConfigs::Get()->GetData("SocketHistory", "");
    QStringList histories = historyData.split(";");

    ui->socketBox->clear();
    foreach (auto history, histories)
    {
        if (history.contains(":"))
            ui->socketBox->addItem(history);
    }
}

void MainWindow::OnProcessStatusChanged(int percentage, QString message)
{
    if (!m_loadingDevice->isVisible())
        m_loadingDevice->ShowProgress("Connect to device...");

    m_loadingDevice->SetProgress(percentage, message);
    ui->statusbar->showMessage(message);
}
