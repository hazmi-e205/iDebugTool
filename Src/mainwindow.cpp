#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "appinfo.h"
#include "logpacket.h"
#include <QSplitter>
#include <QTableView>
#include <QAbstractItemView>
#include <QMimeData>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_devicesModel(nullptr)
    , m_logModel(nullptr)
    , m_ratioTopWidth(0.4f)
    , m_scrollTimer(nullptr)
    , m_installDropEvent(nullptr)
    , m_maxCachedLogs(100)
    , m_maxShownLogs(100)
    , m_scrollInterval(250)
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
    connect(ui->clearBtn, SIGNAL(pressed()), this, SLOT(OnClearClicked()));
    connect(ui->saveBtn, SIGNAL(pressed()), this, SLOT(OnSaveClicked()));

    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(OnScrollTimerTick()));

    SetupDevicesTable();
    SetupLogsTable();
    UpdateStatusbar();

    setAcceptDrops(true);
    ui->installBar->setAlignment(Qt::AlignCenter);

    m_installDropEvent = new CustomKeyFiler();
    ui->installDrop->installEventFilter(m_installDropEvent);
    connect(m_installDropEvent, SIGNAL(pressed()), this, SLOT(OnInstallDropClicked()));
    connect(ui->installBtn, SIGNAL(pressed()), this, SLOT(OnInstallClicked()));

    ui->maxCachedLogs->setText(QString::number(m_maxCachedLogs));
    ui->maxShownLogs->setText(QString::number(m_maxShownLogs));
    ui->scrollInterval->setText(QString::number(m_scrollInterval));
    connect(ui->configureBtn, SIGNAL(pressed()), this, SLOT(OnConfigureClicked()));
}

MainWindow::~MainWindow()
{
    delete m_installDropEvent;
    m_scrollTimer->stop();
    delete m_scrollTimer;
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

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    foreach (const QUrl &url, e->mimeData()->urls()) {
        QString fileName = url.toLocalFile();
        ui->installPath->setText(fileName);
    }
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
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
}

void MainWindow::SetupLogsTable()
{
    if (!m_logModel) {
        m_logModel = new QStandardItemModel();
        ui->logTable->setModel(m_logModel);
        ui->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->logTable->setSelectionMode(QAbstractItemView::MultiSelection);
        ui->logTable->setWordWrap(false);
        ui->logTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        ui->logTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    m_logModel->setHorizontalHeaderItem(0, new QStandardItem("DateTime"));
    m_logModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_logModel->setHorizontalHeaderItem(2, new QStandardItem("ProcessID"));
    m_logModel->setHorizontalHeaderItem(3, new QStandardItem("Type"));
    m_logModel->setHorizontalHeaderItem(4, new QStandardItem("Messages"));
    ui->logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
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
    QList<QStandardItem*> rowData;
    rowData << new QStandardItem(log.getDateTime());
    rowData << new QStandardItem(log.getDeviceName());
    rowData << new QStandardItem(log.getProcessID());
    rowData << new QStandardItem(log.getLogType());
    rowData << new QStandardItem(log.getLogMessage());
    m_logModel->appendRow(rowData);

    while ((unsigned int)m_logModel->rowCount() > m_maxShownLogs)
    {
        m_logModel->removeRow(0);
    }
}

void MainWindow::UpdateStatusbar()
{
    if (m_currentUdid.isEmpty()) {
        ui->statusbar->showMessage("Idle");
    } else {
        QString name = m_infoCache[m_currentUdid]["DeviceName"].toString();
        ui->statusbar->showMessage("Connected to " + (name.length() > 0 ? name : m_currentUdid));
    }
}

void MainWindow::UpdateInfoWidget()
{
    ui->ProductType->setText(m_infoCache[m_currentUdid]["ProductType"].toString());
    ui->OSName->setText(m_infoCache[m_currentUdid]["ProductName"].toString());
    ui->OSVersion->setText(m_infoCache[m_currentUdid]["ProductVersion"].toString());
    ui->CPUArch->setText(m_infoCache[m_currentUdid]["CPUArchitecture"].toString());
    ui->UDID->setText(m_infoCache[m_currentUdid]["UniqueDeviceID"].toString());
}

void MainWindow::OnTopSplitterMoved(int pos, int index)
{
    m_ratioTopWidth = (float)ui->deviceWidget->width() / m_topWidth;
}

void MainWindow::OnDevicesTableClicked(QModelIndex index)
{
    if (index.isValid()) {
        m_currentUdid = index.model()->index(0,0).data().toString();
        UpdateInfoWidget();

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
        UpdateStatusbar();
    }
}

void MainWindow::OnDeviceInfoReceived(QJsonDocument info)
{
    m_infoCache[m_currentUdid] = info;

    UpdateStatusbar();
    UpdateInfoWidget();
    OnUpdateDevices(DeviceBridge::Get()->GetDevices()); //update device name
}

void MainWindow::OnSystemLogsReceived(LogPacket log)
{
    m_liveLogs.push_back(log);
    while ((unsigned int)m_liveLogs.size() > m_maxCachedLogs)
    {
        m_liveLogs.erase(m_liveLogs.begin());
    }

    if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter))
    {
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
    bool autoScroll = state == 0 ? false : true;
    if (autoScroll) {
        m_scrollTimer->start(m_scrollInterval);
    } else {
        m_scrollTimer->stop();
    }
}

void MainWindow::OnClearClicked()
{
    m_liveLogs.clear();
    m_logModel->clear();
    SetupLogsTable();
}

void MainWindow::OnSaveClicked()
{
    qDebug() << "save clicked";
}

void MainWindow::OnInstallDropClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Choose File");
    ui->installPath->setText(filename);
}

void MainWindow::OnInstallClicked()
{
    DeviceBridge::Get()->InstallApp(ui->upgrade->isChecked() ? InstallMode::CMD_UPGRADE : InstallMode::CMD_INSTALL, ui->installPath->text());
}

void MainWindow::OnScrollTimerTick()
{
    ui->logTable->scrollToBottom();
}

void MainWindow::OnConfigureClicked()
{
    m_maxCachedLogs = ui->maxCachedLogs->text().toUInt();
    m_maxShownLogs = ui->maxShownLogs->text().toUInt();
}
