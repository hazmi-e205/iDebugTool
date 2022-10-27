#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "utils.h"
#include "appinfo.h"
#include "logpacket.h"
#include "userconfigs.h"
#include "usbmuxd.h"
#include "crashsymbolicator.h"
#include <QSplitter>
#include <QTableView>
#include <QAbstractItemView>
#include <QMimeData>
#include <QFileDialog>
#include <QJsonValue>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>
#include <QClipboard>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_devicesModel(nullptr)
    , m_logModel(nullptr)
    , m_ratioTopWidth(0.4f)
    , m_scrollTimer(nullptr)
    , m_eventFilter(nullptr)
    , m_maxCachedLogs(UserConfigs::Get()->GetData("MaxCachedLogs", "100").toUInt())
    , m_maxShownLogs(UserConfigs::Get()->GetData("MaxShownLogs", "100").toUInt())
    , m_scrollInterval(UserConfigs::Get()->GetData("ScrollInterval", "250").toUInt())
    , m_textDialog(nullptr)
    , m_imageMounter(nullptr)
    , m_proxyDialog(nullptr)
{
    ui->setupUi(this);

    m_appInfo = new AppInfo(this);
    QMainWindow::setWindowTitle(m_appInfo->GetFullname());
    QMainWindow::setWindowIcon(QIcon(":res/bulb.ico"));
    DeviceBridge::Get()->Init(this);
    connect(DeviceBridge::Get(), SIGNAL(UpdateDevices(QMap<QString,idevice_connection_type>)), this, SLOT(OnUpdateDevices(QMap<QString,idevice_connection_type>)));
    connect(DeviceBridge::Get(), SIGNAL(DeviceConnected()), this, SLOT(OnDeviceConnected()));
    connect(DeviceBridge::Get(), SIGNAL(SystemLogsReceived(LogPacket)), this, SLOT(OnSystemLogsReceived(LogPacket)));
    connect(DeviceBridge::Get(), SIGNAL(InstallerStatusChanged(InstallerMode,QString,int,QString)), this, SLOT(OnInstallerStatusChanged(InstallerMode,QString,int,QString)));
    connect(ui->topSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnTopSplitterMoved(int,int)));
    connect(ui->deviceTable, SIGNAL(clicked(QModelIndex)), this, SLOT(OnDevicesTableClicked(QModelIndex)));
    connect(ui->refreshBtn, SIGNAL(pressed()), this, SLOT(OnRefreshClicked()));
    connect(ui->socketBtn, SIGNAL(pressed()), this, SLOT(OnSocketClicked()));
    connect(ui->searchEdit, SIGNAL(textChanged(QString)), this, SLOT(OnTextFilterChanged(QString)));
    connect(ui->pidEdit, SIGNAL(currentTextChanged(QString)), this, SLOT(OnPidFilterChanged(QString)));
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

    m_eventFilter = new CustomKeyFilter();
    ui->logTable->installEventFilter(m_eventFilter);
    ui->installDrop->installEventFilter(m_eventFilter);
    ui->bundleIds->installEventFilter(m_eventFilter);
    connect(m_eventFilter, SIGNAL(pressed(QObject*)), this, SLOT(OnClickedEvent(QObject*)));
    connect(m_eventFilter, SIGNAL(keyReleased(QObject*,QKeyEvent*)), this, SLOT(OnKeyReleased(QObject*,QKeyEvent*)));
    connect(ui->installBtn, SIGNAL(pressed()), this, SLOT(OnInstallClicked()));
    connect(ui->UninstallBtn, SIGNAL(pressed()), this, SLOT(OnUninstallClicked()));
    connect(ui->installLogs, SIGNAL(pressed()), this, SLOT(OnInstallLogsClicked()));
    connect(ui->bundleIds, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));

    ui->maxCachedLogs->setText(QString::number(m_maxCachedLogs));
    ui->maxShownLogs->setText(QString::number(m_maxShownLogs));
    ui->scrollInterval->setText(QString::number(m_scrollInterval));
    connect(ui->configureBtn, SIGNAL(pressed()), this, SLOT(OnConfigureClicked()));
    connect(ui->proxyBtn, SIGNAL(pressed()), this, SLOT(OnProxyClicked()));
    m_proxyDialog = new ProxyDialog(this);
    m_proxyDialog->UseExisting();

    connect(ui->sleepBtn, SIGNAL(pressed()), this, SLOT(OnSleepClicked()));
    connect(ui->restartBtn, SIGNAL(pressed()), this, SLOT(OnRestartClicked()));
    connect(ui->shutdownBtn, SIGNAL(pressed()), this, SLOT(OnShutdownClicked()));

    m_imageMounter = new ImageMounter(this);
    connect(ui->mounterBtn, SIGNAL(pressed()), this, SLOT(OnImageMounterClicked()));
    connect(ui->screenshotBtn, SIGNAL(pressed()), this, SLOT(OnScreenshotClicked()));

    m_textDialog = new TextViewer(this);
    connect(ui->sysInfoBtn, SIGNAL(pressed()), this, SLOT(OnSystemInfoClicked()));
    connect(ui->appInfoBtn, SIGNAL(pressed()), this, SLOT(OnAppInfoClicked()));

    connect(ui->syncCrashlogsBtn, SIGNAL(pressed()), this, SLOT(OnSyncCrashlogsClicked()));
    connect(DeviceBridge::Get(), SIGNAL(CrashlogsStatusChanged(QString)), this, SLOT(OnCrashlogsStatusChanged(QString)));
    connect(ui->crashlogBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogClicked()));
    connect(ui->dsymBtn, SIGNAL(pressed()), this, SLOT(OnDsymClicked()));
    connect(ui->symbolicateBtn, SIGNAL(pressed()), this, SLOT(OnSymbolicateClicked()));
}

MainWindow::~MainWindow()
{
    CrashSymbolicator::Destroy();
    m_logModel->clear();
    delete m_logModel;
    m_devicesModel->clear();
    delete m_devicesModel;
    ui->logTable->setModel(nullptr);
    ui->deviceTable->setModel(nullptr);
    DeviceBridge::Destroy();
    delete m_eventFilter;
    m_scrollTimer->stop();
    delete m_scrollTimer;
    delete m_appInfo;
    delete m_textDialog;
    delete m_imageMounter;
    delete m_proxyDialog;
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
        ui->deviceTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ui->deviceTable->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        ui->deviceTable->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    }
    m_devicesModel->setHorizontalHeaderItem(0, new QStandardItem("UDID"));
    m_devicesModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_devicesModel->setHorizontalHeaderItem(2, new QStandardItem("Connection"));
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
    ui->deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
}

void MainWindow::SetupLogsTable()
{
    if (!m_logModel) {
        m_logModel = new QStandardItemModel();
        ui->logTable->setModel(m_logModel);
        ui->logTable->setWordWrap(false);
        ui->logTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        ui->logTable->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        ui->logTable->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        ui->logTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
        ui->logTable->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        ui->logTable->setHorizontalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
    }
    m_logModel->setHorizontalHeaderItem(0, new QStandardItem("DateTime"));
    m_logModel->setHorizontalHeaderItem(1, new QStandardItem("DeviceName"));
    m_logModel->setHorizontalHeaderItem(2, new QStandardItem("ProcessID"));
    m_logModel->setHorizontalHeaderItem(3, new QStandardItem("Type"));
    m_logModel->setHorizontalHeaderItem(4, new QStandardItem("Messages"));
    ui->logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeMode::ResizeToContents);
}

void MainWindow::UpdateLogsFilter()
{
    m_logModel->clear();
    SetupLogsTable();
    if (m_currentFilter.isEmpty() && m_pidFilter.isEmpty())
    {
        for (LogPacket& m_Log : m_liveLogs) {
            AddLogToTable(m_Log);
        }
    }
    else
    {
        for (LogPacket& m_Log : m_liveLogs)
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
    if (DeviceBridge::Get()->IsConnected())
    {
        QString device_name = DeviceBridge::Get()->GetDeviceInfo()["DeviceName"].toString();
        device_name = device_name.isEmpty() ? DeviceBridge::Get()->GetCurrentUdid() : device_name;
        ui->statusbar->showMessage("Connected to " + device_name);
    }
    else
    {
        ui->statusbar->showMessage("Idle");
    }
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

void MainWindow::SaveLogMessages(bool savefile)
{
    QModelIndexList indexes = ui->logTable->selectionModel()->selection().indexes();
    int last_row = -1;
    QString data_str = "";
    for (int i = 0; i < indexes.count(); ++i)
    {
        QModelIndex index = indexes.at(i);
        if (last_row > 0 && last_row != index.row())
        {
            data_str += "\n";
        }
        if (index.column() > 0)
        {
            data_str += "\t";
        }
        data_str += index.model()->index(index.row(),index.column()).data().toString();
        last_row = index.row();
    }

    if (savefile)
    {
        QString filename = QFileDialog::getSaveFileName(this, "Save logs file...", "", "Text File (*.txt)");
        QFile f(filename);
        if (f.open(QIODevice::ReadWrite))
        {
            QTextStream stream(&f);
            stream << data_str;
            f.close();
        }
    }
    else
    {
        QApplication::clipboard()->setText(data_str);
    }
}

void MainWindow::OnTopSplitterMoved(int pos, int index)
{
    m_ratioTopWidth = (float)ui->deviceWidget->width() / m_topWidth;
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
    SetupDevicesTable();

    foreach (const auto& udid, devices.keys())
    {
        QList<QStandardItem*> rowData;
        QString name = DeviceBridge::Get()->GetDeviceInfo()["DeviceName"].toString();
        rowData << new QStandardItem(udid);
        rowData << new QStandardItem(name);
        rowData << new QStandardItem(devices[udid] == idevice_connection_type::CONNECTION_NETWORK ? "network" : "usbmuxd");
        m_devicesModel->appendRow(rowData);
    }

    if (!DeviceBridge::Get()->IsConnected())
    {
        if (devices.size() > 0)
        {
            DeviceBridge::Get()->ConnectToDevice(devices.firstKey());
        }
        UpdateStatusbar();
        UpdateInfoWidget();
    }
}

void MainWindow::OnDeviceConnected()
{
    UpdateStatusbar();
    UpdateInfoWidget();
    OnUpdateDevices(DeviceBridge::Get()->GetDevices()); //update device name
}

void MainWindow::OnSystemLogsReceived(LogPacket log)
{
    if (ui->stopCheck->isChecked())
        return;

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

void MainWindow::OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message)
{
    switch (command)
    {
    case InstallerMode::CMD_INSTALL:
        m_installerLogs += (percentage >= 0 ? ("(" + QString::number(percentage) + "%) ") : "") + message + "\n";
        ui->installBar->setValue(percentage);
        break;

    case InstallerMode::CMD_UNINSTALL:
        if (percentage == 100)
            QMessageBox::information(this, "Uninstall Success!", bundleId + " uninstalled.", QMessageBox::Ok);
        break;

    default:
        break;
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
    SaveLogMessages();
}

void MainWindow::OnClickedEvent(QObject* object)
{
    if(object->objectName() == ui->installDrop->objectName())
    {
        QString filename = QFileDialog::getOpenFileName(this, "Choose File");
        ui->installPath->setText(filename);
    }

    if(object->objectName() == ui->bundleIds->objectName())
    {
        ui->bundleIds->clear();
        auto apps = DeviceBridge::Get()->GetInstalledApps();
        for (int idx = 0; idx < apps.array().count(); idx++)
        {
            QString bundle_id = apps[idx]["CFBundleIdentifier"].toString();
            ui->bundleIds->addItem(bundle_id);

            QJsonDocument app_info;
            app_info.setObject(apps[idx].toObject());
            m_installedApps[bundle_id] = app_info;
        }
    }
}

void MainWindow::OnKeyReleased(QObject *object, QKeyEvent *keyEvent)
{
    if(object->objectName() == ui->logTable->objectName())
    {
        if(keyEvent->matches(QKeySequence::Copy))
        {
            SaveLogMessages(false);
        }
    }
}

void MainWindow::OnInstallClicked()
{
    m_installerLogs.clear();
    DeviceBridge::Get()->InstallApp(ui->upgrade->isChecked() ? InstallerMode::CMD_UPGRADE : InstallerMode::CMD_INSTALL, ui->installPath->text());
}

void MainWindow::OnUninstallClicked()
{
    if (!m_choosenBundleId.isEmpty())
    {
        DeviceBridge::Get()->UninstallApp(m_choosenBundleId);
    }
}

void MainWindow::OnInstallLogsClicked()
{
    if(!m_installerLogs.isEmpty())
        m_textDialog->ShowText("Installer Logs", m_installerLogs);
}

void MainWindow::OnScrollTimerTick()
{
    ui->logTable->scrollToBottom();
}

void MainWindow::OnConfigureClicked()
{
    m_maxCachedLogs = ui->maxCachedLogs->text().toUInt();
    m_maxShownLogs = ui->maxShownLogs->text().toUInt();
    m_scrollInterval = ui->scrollInterval->text().toUInt();

    UserConfigs::Get()->SaveData("MaxCachedLogs", ui->maxCachedLogs->text());
    UserConfigs::Get()->SaveData("MaxShownLogs", ui->maxShownLogs->text());
    UserConfigs::Get()->SaveData("ScrollInterval", ui->scrollInterval->text());
}

void MainWindow::OnProxyClicked()
{
    m_proxyDialog->ShowDialog();
}

void MainWindow::OnSleepClicked()
{
    DeviceBridge::Get()->StartDiagnostics(DiagnosticsMode::CMD_SLEEP);
}

void MainWindow::OnShutdownClicked()
{
    DeviceBridge::Get()->StartDiagnostics(DiagnosticsMode::CMD_SHUTDOWN);
}

void MainWindow::OnRestartClicked()
{
    DeviceBridge::Get()->StartDiagnostics(DiagnosticsMode::CMD_RESTART);
}

void MainWindow::OnBundleIdChanged(QString text)
{
    m_choosenBundleId = text;
    auto app_info = m_installedApps[text];
    ui->AppName->setText(app_info["CFBundleName"].toString());
    ui->AppVersion->setText(app_info["CFBundleShortVersionString"].toString());
    ui->AppSigner->setText(app_info["SignerIdentity"].toString());
}

void MainWindow::OnSystemInfoClicked()
{
    if(DeviceBridge::Get()->IsConnected())
        m_textDialog->ShowText("System Information", DeviceBridge::Get()->GetDeviceInfo().toJson());
}

void MainWindow::OnAppInfoClicked()
{
    if(!m_choosenBundleId.isEmpty())
        m_textDialog->ShowText("App Information",m_installedApps[m_choosenBundleId].toJson());
}

void MainWindow::OnImageMounterClicked()
{
    auto mounted = DeviceBridge::Get()->GetMountedImages();
    if (mounted.length() > 0)
    {
        QMessageBox::information(this, "Disk Mounted!", "Developer disk image mounted", QMessageBox::Ok);
    }
    else
    {
        m_imageMounter->ShowDialog();
    }
}

void MainWindow::OnScreenshotClicked()
{
    if (!DeviceBridge::Get()->IsImageMounted())
    {
        m_imageMounter->ShowDialog(true);
        m_imageMounter->exec();
        if (!DeviceBridge::Get()->IsImageMounted()) return;
    }


    QString imagePath = GetDirectory(DIRECTORY_TYPE::SCREENSHOT) + "Screenshot_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".png";
    if (DeviceBridge::Get()->Screenshot(imagePath))
    {
        ui->statusbar->showMessage("Screenshot saved to '" + imagePath + "'!");

        QMessageBox msgBox(QMessageBox::Question, "Screenshot", "Screenshot has been taken!\n" + imagePath);
        QPushButton *shotButton = msgBox.addButton("Take another shot!", QMessageBox::ButtonRole::ActionRole);
        QPushButton *dirButton = msgBox.addButton("Go to directory...", QMessageBox::ButtonRole::ActionRole);
        msgBox.addButton(QMessageBox::StandardButton::Close);
        msgBox.exec();
        if (msgBox.clickedButton() == shotButton)
        {
            OnScreenshotClicked();
        }
        else if (msgBox.clickedButton() == dirButton)
        {
            QDesktopServices::openUrl(GetDirectory(DIRECTORY_TYPE::SCREENSHOT));
        }
    }
}

void MainWindow::OnSocketClicked()
{
    if (ui->socketBtn->text() == "Connect")
    {
        QString text = FindRegex(ui->socketBox->currentText(), "(\\d+\\.\\d+\\.\\d+\\.\\d+):(\\d+)");
        QStringList text_split = text.split(":");
        if (text_split.length() == 2)
        {
            if (usbmuxd_connect_remote(text_split[0].toUtf8().data(), text_split[1].toUInt()) >= 0)
                ui->socketBtn->setText("Disconnect");
            else
                QMessageBox::critical(this, "Error", "Error: fail to connect '" + text + "' device via socket", QMessageBox::Ok);
        }
    }
    else
    {
        DeviceBridge::Get()->ResetConnection();
        usbmuxd_disconnect_remote();
        ui->socketBtn->setText("Connect");
    }

    //update devce list
    OnUpdateDevices(DeviceBridge::Get()->GetDevices());
}

void MainWindow::OnSyncCrashlogsClicked()
{
    qDebug() << DeviceBridge::Get()->CopyCrashlogToDir(GetDirectory(DIRECTORY_TYPE::CRASHLOGS));
}

void MainWindow::OnCrashlogsStatusChanged(QString text)
{
    ui->crashlogsOut->appendPlainText(text);
}

void MainWindow::OnCrashlogClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Choose crashlog");
    ui->crashlogEdit->setText(filename);
}

void MainWindow::OnDsymClicked()
{
    QString filename = QFileDialog::getExistingDirectory(this, "Choose dSYM");
    ui->dsymEdit->setText(filename);
}

void MainWindow::OnSymbolicateClicked()
{
    QString crashpath = ui->crashlogEdit->text();
    QString dsympath = ui->dsymEdit->text();
    QString sybolicated = CrashSymbolicator::Get()->Proccess(crashpath, dsympath);
    ui->crashlogsOut->setPlainText(sybolicated);
}
