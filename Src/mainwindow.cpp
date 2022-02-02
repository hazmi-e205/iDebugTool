#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "appinfo.h"
#include "logpacket.h"
#include <QSplitter>
#include <QTableView>
#include <QAbstractItemView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_ratioTopWidth(0.4f)
    , m_devicesModel(nullptr)
    , m_logModel(nullptr)
    , m_autoScroll(false)
{
    ui->setupUi(this);

    m_appInfo = new AppInfo(this);
    QMainWindow::setWindowTitle(m_appInfo->GetFullname());
    QMainWindow::setWindowIcon(QIcon(":res/bulb.ico"));
    DeviceBridge::Get()->Init(this);
    connect(DeviceBridge::Get(), SIGNAL(UpdateDevices(std::map<QString,idevice_connection_type>)), this, SLOT(OnUpdateDevices(std::map<QString,idevice_connection_type>)));
    connect(DeviceBridge::Get(), SIGNAL(DeviceInfoReceived(QJsonDocument)), this, SLOT(OnDeviceInfoReceived(QJsonDocument)));
    connect(DeviceBridge::Get(), SIGNAL(SystemLogsReceived(LogPacket)), this, SLOT(OnSystemLogsReceived(LogPacket)));
    connect(ui->topSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnTopSplitterMoved(int,int)));
    connect(ui->deviceTable, SIGNAL(clicked(QModelIndex)), this, SLOT(OnDevicesTableClicked(QModelIndex)));
    connect(ui->refreshBtn, SIGNAL(pressed()), this, SLOT(OnRefreshClicked()));
    connect(ui->searchEdit, SIGNAL(textChanged(QString)), this, SLOT(OnTextFilterChanged(QString)));
    connect(ui->pidEdit, SIGNAL(textChanged(QString)), this, SLOT(OnPidFilterChanged(QString)));
    connect(ui->excludeEdit, SIGNAL(textChanged(QString)), this, SLOT(OnExcludeFilterChanged(QString)));
    connect(ui->scrollCheck, SIGNAL(stateChanged(int)), this, SLOT(OnAutoScrollChecked(int)));

    SetupDevicesTable();
    SetupLogsTable();
}

MainWindow::~MainWindow()
{
    DeviceBridge::Destroy();
    delete m_appInfo;
    delete m_devicesModel;
    delete m_logModel;
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    //qDebug() << "gap w:" << event->size().width() - (ui->deviceWidget->width() + ui->featureWidget->width());
    m_topWidth = (float)(event->size().width() - 41); //hardcoded number from log above while resize

    int newTopSize = (int)(m_topWidth * m_ratioTopWidth);
    ui->deviceWidget->resize(newTopSize, 0);

    newTopSize = m_topWidth - newTopSize;
    ui->featureWidget->resize(newTopSize, 0);
}

void MainWindow::SetupDevicesTable()
{
    if (!m_devicesModel) {
        m_devicesModel = new QStandardItemModel();
        ui->deviceTable->setModel(m_devicesModel);
        ui->deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    }
    m_devicesModel->setHorizontalHeaderItem(0, new QStandardItem("UDID"));
    m_devicesModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_devicesModel->setHorizontalHeaderItem(2, new QStandardItem("Connection"));
}

void MainWindow::SetupLogsTable()
{
    if (!m_logModel) {
        m_logModel = new QStandardItemModel();
        ui->logTable->setModel(m_logModel);
        ui->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->logTable->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    m_logModel->setHorizontalHeaderItem(0, new QStandardItem("DateTime"));
    m_logModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_logModel->setHorizontalHeaderItem(2, new QStandardItem("ProcessID"));
    m_logModel->setHorizontalHeaderItem(3, new QStandardItem("Type"));
    m_logModel->setHorizontalHeaderItem(4, new QStandardItem("Messages"));
}

void MainWindow::UpdateLogsFilter()
{
    m_logModel->clear();
    SetupLogsTable();
    if (m_currentFilter.isEmpty() && m_pidFilter.isEmpty())
    {
        for (LogPacket m_Log : m_liveLogs) {
            AddLogToTable(m_Log);
        }
    }
    else
    {
        for (LogPacket m_Log : m_liveLogs)
        {
            if (m_Log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter)) {
                AddLogToTable(m_Log);
            }
        }
    }
}

void MainWindow::AddLogToTable(LogPacket log)
{
    if (m_autoScroll) {
        ui->logTable->scrollToBottom();
    }
    QList<QStandardItem*> rowData;
    rowData << new QStandardItem(log.getDateTime());
    rowData << new QStandardItem(log.getDeviceName());
    rowData << new QStandardItem(log.getProcessID());
    rowData << new QStandardItem(log.getLogType());
    rowData << new QStandardItem(log.getLogMessage());
    m_logModel->appendRow(rowData);
}

void MainWindow::OnTopSplitterMoved(int pos, int index)
{
    m_ratioTopWidth = (float)ui->deviceWidget->width() / m_topWidth;
}

void MainWindow::OnDevicesTableClicked(QModelIndex index)
{
    if (index.isValid()) {
        m_currentUdid = index.model()->index(0,0).data().toString();
        auto type = index.model()->index(0,2).data().toString() == "network" ? idevice_connection_type::CONNECTION_NETWORK : idevice_connection_type::CONNECTION_USBMUXD;
        DeviceBridge::Get()->ConnectToDevice(m_currentUdid, type);
    }
}

void MainWindow::OnRefreshClicked()
{
    OnUpdateDevices(DeviceBridge::Get()->GetDevices());
}

void MainWindow::OnUpdateDevices(std::map<QString, idevice_connection_type> devices)
{
    m_devicesModel->clear();
    SetupDevicesTable();

    //m_devices = devices;
    bool currentUdidFound = false;

    for(std::map<QString, idevice_connection_type>::iterator it = devices.begin(); it != devices.end(); ++it) {
        QList<QStandardItem*> rowData;
        QString udid = it->first;
        QString name = m_infoCache[udid]["DeviceName"].toString();
        rowData << new QStandardItem(udid);
        rowData << new QStandardItem(name);
        rowData << new QStandardItem(it->second == idevice_connection_type::CONNECTION_NETWORK ? "network" : "usbmuxd");
        m_devicesModel->appendRow(rowData);
        if (!currentUdidFound) {
            currentUdidFound = m_currentUdid == udid ? true : false;
        }
    }

    if(devices.size() == 0 || !currentUdidFound)
    {
        m_currentUdid = "";
        ui->statusbar->showMessage("Idle");
    }
}

void MainWindow::OnDeviceInfoReceived(QJsonDocument info)
{
    m_infoCache[m_currentUdid] = info;

    QString name = m_infoCache[m_currentUdid]["DeviceName"].toString();
    ui->statusbar->showMessage("Connected to " + (name.length() > 0 ? name : m_currentUdid));
}

void MainWindow::OnSystemLogsReceived(LogPacket log)
{
    m_liveLogs.push_back(log);
    if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter)) {
        AddLogToTable(log);
    }
}

void MainWindow::OnTextFilterChanged(QString text)
{
    m_currentFilter = text;
    UpdateLogsFilter();
}

void MainWindow::OnPidFilterChanged(QString text)
{
    m_pidFilter = text;
    UpdateLogsFilter();
}

void MainWindow::OnExcludeFilterChanged(QString text)
{
    m_excludeFilter = text;
    UpdateLogsFilter();
}

void MainWindow::OnAutoScrollChecked(int state)
{
    m_autoScroll = state == 0 ? false : true;
}
