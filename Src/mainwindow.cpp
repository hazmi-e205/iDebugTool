#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "utils.h"
#include "appinfo.h"
#include "logpacket.h"
#include "userconfigs.h"
#include "usbmuxd.h"
#include "crashsymbolicator.h"
#include "asyncmanager.h"
#include "customgrid.h"
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

#define SYSTEM_LIST "lockdownd|crash_mover|securityd|trustd|remindd|CommCenter|kernel|locationd|mobile_storage_proxy|wifid|dasd|UserEventAgent|exchangesyncd|runningboardd|powerd|mDNSResponder|symptomsd|WirelessRadioManagerd|nsurlsessiond|searchpartyd|mediaserverd|homed|rapportd|powerlogHelperd|aggregated|cloudd|keybagd|sharingd|tccd|bluetoothd|identityservicesd|nearbyd|PowerUIAgent|maild|timed|syncdefaultsd|distnoted|accountsd|analyticsd|apsd|ProtectedCloudKeySyncing|testmanagerd|backboardd|SpringBoard|familycircled|useractivityd|contextstored|Preferences|passd|IDSRemoteURLConnectionAgent|nfcd|coreduetd|duetexpertd|navd|destinationd|com.apple.Safari.SafeBrowsing.Service|dataaccessd|HeuristicInterpreter|pasted|suggestd|appstored|rtcreportingd|awdd|parsec-fbf|lsd|chronod|com.apple.WebKit.Networking|callservicesd|druid|kbd|mediaremoted|watchdogd|MTLCompilerService|itunesstored|EnforcementService|gamed|adprivacyd|profiled|CAReportingService|assistantd|itunescloudd|parsecd|osanalyticshelper|triald|deleted|Spotlight|searchd|mobileassetd|contactsdonationagent|followupd|containermanagerd|ThreeBarsXPCService|routined|accessoryd|healthd|SafariBookmarksSyncAgent|ScreenTimeAgent|gpsd"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_devicesModel(nullptr)
    , m_ratioTopWidth(0.4f)
    , m_scrollTimer(nullptr)
    , m_eventFilter(nullptr)
    , m_maxShownLogs(UserConfigs::Get()->GetData("MaxShownLogs", "100").toUInt())
    , m_scrollInterval(UserConfigs::Get()->GetData("ScrollInterval", "250").toUInt())
    , m_textDialog(nullptr)
    , m_imageMounter(nullptr)
    , m_proxyDialog(nullptr)
    , m_aboutDialog(nullptr)
    , m_loading(nullptr)
    , m_table(nullptr)
{
    ui->setupUi(this);

    AsyncManager::Get()->Init(4);
    m_appInfo = new AppInfo(this);
    QMainWindow::setWindowTitle(m_appInfo->GetFullname());
    QMainWindow::setWindowIcon(QIcon(":res/bulb.ico"));
    DeviceBridge::Get()->Init(this);
    connect(DeviceBridge::Get(), SIGNAL(UpdateDevices(QMap<QString,idevice_connection_type>)), this, SLOT(OnUpdateDevices(QMap<QString,idevice_connection_type>)));
    connect(DeviceBridge::Get(), SIGNAL(DeviceConnected()), this, SLOT(OnDeviceConnected()));
    connect(DeviceBridge::Get(), SIGNAL(SystemLogsReceived(LogPacket)), this, SLOT(OnSystemLogsReceived(LogPacket)));
    connect(DeviceBridge::Get(), SIGNAL(InstallerStatusChanged(InstallerMode,QString,int,QString)), this, SLOT(OnInstallerStatusChanged(InstallerMode,QString,int,QString)));
    connect(DeviceBridge::Get(), SIGNAL(ProcessStatusChanged(int,QString)), this, SLOT(OnProcessStatusChanged(int,QString)));
    connect(DeviceBridge::Get(), SIGNAL(MessagesReceived(MessagesType,QString)), this, SLOT(OnMessagesReceived(MessagesType,QString)));
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
    connect(ui->updateBtn, SIGNAL(pressed()), this, SLOT(OnUpdateClicked()));

    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(OnScrollTimerTick()));

    m_aboutDialog = new AboutDialog(&m_appInfo, this);
    connect(ui->aboutBtn, SIGNAL(pressed()), m_aboutDialog, SLOT(show()));

    m_loading = new LoadingDialog(this);
    m_eventFilter = new CustomKeyFilter();
    ui->statusbar->showMessage("Idle");
    SetupDevicesTable();
    SetupLogsTable();
    RefreshSocketList();

    setAcceptDrops(true);
    ui->installBar->setAlignment(Qt::AlignCenter);

    ui->installDrop->installEventFilter(m_eventFilter);
    ui->bundleIds->installEventFilter(m_eventFilter);
    ui->pidEdit->installEventFilter(m_eventFilter);
    connect(m_eventFilter, SIGNAL(pressed(QObject*)), this, SLOT(OnClickedEvent(QObject*)));
    connect(ui->installBtn, SIGNAL(pressed()), this, SLOT(OnInstallClicked()));
    connect(ui->UninstallBtn, SIGNAL(pressed()), this, SLOT(OnUninstallClicked()));
    connect(ui->installLogs, SIGNAL(pressed()), this, SLOT(OnInstallLogsClicked()));
    connect(ui->bundleIds, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));

    ui->maxShownLogs->setText(QString::number(m_maxShownLogs));
    ui->scrollInterval->setText(QString::number(m_scrollInterval));
    connect(ui->configureBtn, SIGNAL(pressed()), this, SLOT(OnConfigureClicked()));
    connect(ui->proxyBtn, SIGNAL(pressed()), this, SLOT(OnProxyClicked()));
    connect(ui->excludeSytemBtn, SIGNAL(pressed()), this, SLOT(OnExcludeSystemLogListClicked()));
    m_proxyDialog = new ProxyDialog(this);
    m_proxyDialog->UseExisting();

    connect(ui->excludeSystemCheck, SIGNAL(stateChanged(int)), this, SLOT(OnExcludeSystemLogsChecked(int)));
    bool IsExcludeSystem = UserConfigs::Get()->GetBool("ExcludeSystemLogs", true);
    ui->excludeSystemCheck->setCheckState(IsExcludeSystem ? Qt::Checked : Qt::Unchecked);
    ExcludeSystemLogs();

    connect(ui->sleepBtn, SIGNAL(pressed()), this, SLOT(OnSleepClicked()));
    connect(ui->restartBtn, SIGNAL(pressed()), this, SLOT(OnRestartClicked()));
    connect(ui->shutdownBtn, SIGNAL(pressed()), this, SLOT(OnShutdownClicked()));

    m_imageMounter = new ImageMounter(this);
    connect(ui->mounterBtn, SIGNAL(pressed()), this, SLOT(OnImageMounterClicked()));
    connect(ui->screenshotBtn, SIGNAL(pressed()), this, SLOT(OnScreenshotClicked()));
    connect(DeviceBridge::Get(), SIGNAL(ScreenshotReceived(QString)), this, SLOT(OnScreenshotReceived(QString)));

    m_textDialog = new TextViewer(this);
    connect(ui->sysInfoBtn, SIGNAL(pressed()), this, SLOT(OnSystemInfoClicked()));
    connect(ui->appInfoBtn, SIGNAL(pressed()), this, SLOT(OnAppInfoClicked()));

    connect(ui->syncCrashlogsBtn, SIGNAL(pressed()), this, SLOT(OnSyncCrashlogsClicked()));
    connect(DeviceBridge::Get(), SIGNAL(CrashlogsStatusChanged(QString)), this, SLOT(OnCrashlogsStatusChanged(QString)));
    connect(ui->crashlogBtn, SIGNAL(pressed()), this, SLOT(OnCrashlogClicked()));
    connect(ui->dsymBtn, SIGNAL(pressed()), this, SLOT(OnDsymClicked()));
    connect(ui->dwarfBtn, SIGNAL(pressed()), this, SLOT(OnDwarfClicked()));
    connect(ui->symbolicateBtn, SIGNAL(pressed()), this, SLOT(OnSymbolicateClicked()));
    connect(CrashSymbolicator::Get(), SIGNAL(SymbolicateResult(QString,bool)), this, SLOT(OnSymbolicateResult(QString,bool)));

    m_appInfo->CheckUpdate([&](QString changelogs, QString url){
        if (changelogs.isEmpty() && url.isEmpty())
        {
            ui->updateBtn->setText(" You're up to date!");
            ui->updateBtn->setIcon(QIcon(":res/Assets/GreenCheck.png"));
        }
        else
        {
            ui->updateBtn->setText(" New version available!");
            ui->updateBtn->setIcon(QIcon(":res/Assets/YellowInfo.png"));
        }
    });

    MassStylesheet(STYLE_TYPE::ROUNDED_BUTTON_LIGHT, QList<QWidget*>()
                   << ui->aboutBtn
                   << ui->refreshBtn
                   << ui->sysInfoBtn
                   << ui->excludeSytemBtn
                   << ui->proxyBtn
                   << ui->configureBtn
                   << ui->installBtn
                   << ui->installLogs
                   << ui->appInfoBtn
                   << ui->UninstallBtn
                   << ui->syncCrashlogsBtn
                   << ui->crashlogsDirBtn
                   << ui->crashlogBtn
                   << ui->dsymBtn
                   << ui->dwarfBtn
                   << ui->symbolicateBtn
                   << ui->socketBtn
                   << ui->saveBtn
                   << ui->clearBtn);

    MassStylesheet(STYLE_TYPE::ROUNDED_EDIT_LIGHT, QList<QWidget*>()
                   << ui->UDID
                   << ui->maxShownLogs
                   << ui->scrollInterval
                   << ui->installPath
                   << ui->AppSigner
                   << ui->crashlogEdit
                   << ui->dsymEdit
                   << ui->searchEdit
                   << ui->excludeEdit);

    MassStylesheet(STYLE_TYPE::ROUNDED_COMBOBOX_LIGHT, QList<QWidget*>()
                   << ui->bundleIds
                   << ui->socketBox
                   << ui->pidEdit);
}

MainWindow::~MainWindow()
{
    CrashSymbolicator::Destroy();
    m_devicesModel->clear();
    delete m_devicesModel;
    delete m_table;
    delete m_dataModel;
    ui->deviceTable->setModel(nullptr);
    delete m_eventFilter;
    m_scrollTimer->stop();
    delete m_scrollTimer;
    delete m_appInfo;
    delete m_textDialog;
    delete m_imageMounter;
    delete m_proxyDialog;
    delete m_aboutDialog;
    delete m_loading;
    AsyncManager::Destroy();
    delete ui;
#if defined(DESTROY_DEVICE_BRIDGE)
    DeviceBridge::Destroy();
#else
    QApplication::quit();
#endif
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
    if (!m_table)
    {
        m_dataModel = new QicsDataModelDefault(0, 0);
        m_table = new QicsTable(0, 0, CustomGrid::createGrid, 0, m_dataModel);
        ui->logLayout->addWidget(m_table);
    }
    m_table->addColumns(5);
    m_table->addRows(1);
    m_table->columnHeaderRef().cellRef(0,0).setLabel("DateTime");
    m_table->columnHeaderRef().cellRef(0,1).setLabel("DeviceName");
    m_table->columnHeaderRef().cellRef(0,2).setLabel("ProcessID");
    m_table->columnHeaderRef().cellRef(0,3).setLabel("Type");
    m_table->columnHeaderRef().cellRef(0,4).setLabel("Messages");
    m_table->columnHeaderRef().cellRef(0,4).setWidthInPixels(1000);
    //m_table->setSelectionPolicy(Qics::QicsSelectionPolicy::SelectMultipleRow);
    m_table->setReadOnly(true);
}

void MainWindow::UpdateLogsFilter()
{
    m_mutex.lock();
    m_table->clearTable();
    SetupLogsTable();

    QList<LogPacket> logs;
    for (LogPacket& m_Log : m_liveLogs)
    {
        if (m_Log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_excludeSystemFilter)) {
            logs << m_Log;
        }
    }
    AddLogToTable(logs);
    m_mutex.unlock();
}

void MainWindow::AddLogToTable(LogPacket log)
{
    if (!m_dataModel->isRowEmpty(0))
        m_dataModel->addRows(1);

    auto idx = m_dataModel->numRows() - 1;
    m_dataModel->setItem(idx, 0, QicsDataString(log.getDateTime()));
    m_dataModel->setItem(idx, 1, QicsDataString(log.getDeviceName()));
    m_dataModel->setItem(idx, 2, QicsDataString(log.getProcessID()));
    m_dataModel->setItem(idx, 3, QicsDataString(log.getLogType()));

    auto lines = log.getLogMessage().split('\n');
    if (lines.count() > 0)
    {
        for (quint64 line_idx = 0; line_idx < lines.count(); line_idx++)
        {
            if (line_idx > 0)
            {
                m_dataModel->addRows(1);
            }
            m_dataModel->setItem(idx + line_idx, 4, QicsDataString(lines[line_idx]));
        }
    }
    else
    {
        m_dataModel->setItem(idx, 4, QicsDataString(log.getLogMessage()));
    }

    if (m_dataModel->numRows() > m_maxShownLogs)
    {
        quint64 deleteCount = m_dataModel->numRows() - m_maxShownLogs;
        m_dataModel->deleteRows(deleteCount, 0);
    }
}

void MainWindow::AddLogToTable(QList<LogPacket> logs)
{
    if (logs.count() == 0)
        return;

    m_dataModel->addRows(m_dataModel->isRowEmpty(0) ? logs.count()-1 : logs.count());
    auto first_idx = m_dataModel->numRows() - logs.count() - 1;
    for (int idx = 0; idx <logs.count(); idx++)
    {
        m_dataModel->setItem(first_idx + idx, 0, QicsDataString(logs[idx].getDateTime()));
        m_dataModel->setItem(first_idx + idx, 1, QicsDataString(logs[idx].getDeviceName()));
        m_dataModel->setItem(first_idx + idx, 2, QicsDataString(logs[idx].getProcessID()));
        m_dataModel->setItem(first_idx + idx, 3, QicsDataString(logs[idx].getLogType()));

        auto lines = logs[idx].getLogMessage().split('\n');
        if (lines.count() > 0)
        {
            for (quint64 line_idx = 0; line_idx < lines.count(); line_idx++)
            {
                if (line_idx > 0)
                {
                    m_dataModel->addRows(1);
                }
                m_dataModel->setItem(idx + line_idx, 4, QicsDataString(lines[line_idx]));
            }
        }
        else
        {
            m_dataModel->setItem(idx, 4, QicsDataString(logs[idx].getLogMessage()));
        }
    }

    if (m_dataModel->numRows() > m_maxShownLogs)
    {
        quint64 deleteCount = m_dataModel->numRows() - m_maxShownLogs;
        m_dataModel->deleteRows(deleteCount, 0);
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

void MainWindow::ExcludeSystemLogs()
{
    QString excludeData = UserConfigs::Get()->GetData("SystemLogList", SYSTEM_LIST);
    if (ui->excludeSystemCheck->isChecked())
    {
        m_excludeSystemFilter = excludeData;
    }
    else
    {
        m_excludeSystemFilter.clear();
    }
    UpdateLogsFilter();

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
        ui->statusbar->showMessage("Idle");
        if (devices.size() > 0)
        {
            DeviceBridge::Get()->ConnectToDevice(devices.firstKey());
        }
        UpdateInfoWidget();
    }
}

void MainWindow::OnDeviceConnected()
{
    UpdateInfoWidget();
    OnUpdateDevices(DeviceBridge::Get()->GetDevices()); //update device name
}

void MainWindow::OnSystemLogsReceived(LogPacket log)
{
    if (ui->stopCheck->isChecked())
        return;

    m_liveLogs.push_back(log);
    while ((unsigned int)m_liveLogs.size() > m_maxShownLogs)
    {
        m_liveLogs.erase(m_liveLogs.begin());
    }

    if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_excludeSystemFilter))
    {
        AddLogToTable(log);
    }
}

void MainWindow::OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message)
{
    switch (command)
    {
    case InstallerMode::CMD_INSTALL:
        {
            QString messages = (percentage >= 0 ? ("(" + QString::number(percentage) + "%) ") : "") + message;
            m_installerLogs += m_installerLogs.isEmpty() ? messages : ("\n" + messages);
            ui->installBar->setFormat("%p% " + message);
            ui->installBar->setValue(percentage);
            if (m_textDialog->isActiveWindow() && m_textDialog->windowTitle().contains("Installer"))
                m_textDialog->AppendText(messages);
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

void MainWindow::OnExcludeSystemLogsChecked(int state)
{
    UserConfigs::Get()->SaveData("ExcludeSystemLogs", state == 0 ? false : true);
    ExcludeSystemLogs();
}

void MainWindow::OnClearClicked()
{
    bool turnBack = false;
    if (!ui->stopCheck->isChecked())
    {
        turnBack = true;
        ui->stopCheck->click();
    }

    m_table->clearTable();
    m_liveLogs.clear();
    SetupLogsTable();

    if (turnBack)
        ui->stopCheck->click();
}

void MainWindow::OnSaveClicked()
{
    bool turnBack = false;
    if (!ui->stopCheck->isChecked())
    {
        turnBack = true;
        ui->stopCheck->click();
    }

    int rowCount = m_table->selectionList(true)->rows().count();
    int columnCount = m_table->selectionList(true)->columns().count();
    if (rowCount <= 1 && columnCount <= 1)
        m_table->selectAll();

    m_table->copy();
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Log", this, "Text File (*.txt)");
    QFile f(filepath);
    if (f.open(QIODevice::WriteOnly))
    {
        QTextStream stream(&f);
        stream << clipboard->text();
        f.close();
    }

    if (turnBack)
        ui->stopCheck->click();
    m_table->clearSelectionList();
}

void MainWindow::OnClickedEvent(QObject* object)
{
    if(object->objectName() == ui->installDrop->objectName())
    {
        QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "App", this);
        ui->installPath->setText(filepath);
    }

    if(object->objectName() == ui->bundleIds->objectName() || object->objectName() == ui->pidEdit->objectName())
    {
        if (IsInstalledUpdated())
        {
            ui->bundleIds->clear();
            ui->bundleIds->addItems(m_installedApps.keys());

            QString old_string = ui->pidEdit->currentText();
            ui->pidEdit->clear();
            foreach (auto appinfo, m_installedApps)
            {
                QString bundle_id = appinfo["CFBundleExecutable"].toString();
                ui->pidEdit->addItem(bundle_id);
            }
            ui->pidEdit->setEditText(old_string);
        }
    }
}

bool MainWindow::IsInstalledUpdated()
{
    auto apps = DeviceBridge::Get()->GetInstalledApps();
    if (apps.array().size() != m_installedApps.size())
    {
        m_installedApps.clear();
        for (int idx = 0; idx < apps.array().count(); idx++)
        {
            QString bundle_id = apps[idx]["CFBundleIdentifier"].toString();
            QJsonDocument app_info;
            app_info.setObject(apps[idx].toObject());
            m_installedApps[bundle_id] = app_info;
        }
        return true;
    }
    return false;
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
    int max_value = m_table->verticalScrollBar()->maximum();
    int value = m_table->verticalScrollBar()->value();
    if (max_value != value)
    {
        if (m_lastAutoScroll && (m_lastMaxScroll == max_value))
        {
            ui->scrollCheck->setCheckState(Qt::CheckState::Unchecked);
            m_lastAutoScroll = false;
        }

        if (ui->scrollCheck->isChecked())
        {
            m_table->verticalScrollBar()->setValue(max_value);
            m_lastAutoScroll = true;
        }
    }
    m_lastMaxScroll = max_value;
}

void MainWindow::OnConfigureClicked()
{
    m_maxShownLogs = ui->maxShownLogs->text().toUInt();
    m_scrollInterval = ui->scrollInterval->text().toUInt();

    UserConfigs::Get()->SaveData("MaxShownLogs", ui->maxShownLogs->text());
    UserConfigs::Get()->SaveData("ScrollInterval", ui->scrollInterval->text());
    ExcludeSystemLogs();
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
    DeviceBridge::Get()->Screenshot(imagePath);
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
        usbmuxd_disconnect_remote();
        ui->socketBtn->setText("Connect");
    }

    //update devce list
    OnUpdateDevices(DeviceBridge::Get()->GetDevices());
}

void MainWindow::OnSyncCrashlogsClicked()
{
    DeviceBridge::Get()->SyncCrashlogs(GetDirectory(DIRECTORY_TYPE::CRASHLOGS));
}

void MainWindow::OnCrashlogsStatusChanged(QString text)
{
    ui->crashlogsOut->appendPlainText(text);
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
    CrashSymbolicator::Get()->Proccess(crashpath, dsympath);
}

void MainWindow::OnSymbolicateResult(QString messages, bool error)
{
    if (error)
    {
        QMessageBox::critical(this, "Error", messages, QMessageBox::Ok);
        ui->statusbar->showMessage("Symbolication failed because " + messages + "!");
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
        ui->statusbar->showMessage("Symbolicated file saved to '" + messages + "'!");
    }
}

void MainWindow::OnScreenshotReceived(QString imagePath)
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

void MainWindow::OnExcludeSystemLogListClicked()
{
    QString excludeData = UserConfigs::Get()->GetData("SystemLogList", SYSTEM_LIST);
    QStringList excludes = excludeData.split("|");

    excludeData = excludes.join('\n');
    m_textDialog->ShowText("System Logs Exclude List", excludeData, false, [](QString data){
        QStringList excludes = data.split("\n");
        UserConfigs::Get()->SaveData("SystemLogList", excludes.join('|'));
    });
}

void MainWindow::OnProcessStatusChanged(int percentage, QString message)
{
    if (!m_loading->isActiveWindow())
        m_loading->ShowProgress("Connect to device...");

    m_loading->SetProgress(percentage, message);
    ui->statusbar->showMessage(message);
}

void MainWindow::OnUpdateClicked()
{
    m_appInfo->CheckUpdate([&](QString changelogs, QString url)
    {
        if (changelogs.isEmpty())
            return;

        m_textDialog->ShowText("New Versions", changelogs, true, [url](QString){
            QDesktopServices::openUrl(QUrl(url));
        }, "Go to download...");
    });
}

void MainWindow::OnMessagesReceived(MessagesType type, QString messages)
{
    switch (type) {
    case MessagesType::MSG_ERROR:
        QMessageBox::critical(this, "Error", messages, QMessageBox::Ok);
        break;
    case MessagesType::MSG_WARN:
        QMessageBox::warning(this, "Warning", messages, QMessageBox::Ok);
        break;
    default:
        QMessageBox::information(this, "Information", messages, QMessageBox::Ok);
        break;
    }
}
