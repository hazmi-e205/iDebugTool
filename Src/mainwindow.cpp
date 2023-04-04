#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "utils.h"
#include "appinfo.h"
#include "logpacket.h"
#include "userconfigs.h"
#include "usbmuxd.h"
#include "crashsymbolicator.h"
#include "recodesigner.h"
#include "asyncmanager.h"
#include <QSplitter>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QMimeData>
#include <QFileDialog>
#include <QJsonValue>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QVariant>
#include <QMenu>
#include <QAction>
#include <QFontMetricsF>

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
    , m_paddinglogs("0|0|0|0")
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
    connect(ui->saveOutputBtn, SIGNAL(pressed()), this, SLOT(OnSaveOutputClicked()));
    connect(ui->clearOutputBtn, SIGNAL(pressed()), this, SLOT(OnClearOutputClicked()));
    connect(ui->updateBtn, SIGNAL(pressed()), this, SLOT(OnUpdateClicked()));
    connect(ui->bottomWidget, SIGNAL(currentChanged(int)), this, SLOT(OnBottomTabChanged(int)));

    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(OnScrollTimerTick()));
    if (ui->scrollCheck->isChecked())
        m_scrollTimer->start(m_scrollInterval);

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
    ui->pidEdit->addItems(QStringList() << "By user apps only" << "Related to user apps");
    ui->pidEdit->setCurrentIndex(0);
    connect(m_eventFilter, SIGNAL(pressed(QObject*)), this, SLOT(OnClickedEvent(QObject*)));
    connect(ui->installBtn, SIGNAL(pressed()), this, SLOT(OnInstallClicked()));
    connect(ui->UninstallBtn, SIGNAL(pressed()), this, SLOT(OnUninstallClicked()));
    connect(ui->bundleIds, SIGNAL(textActivated(QString)), this, SLOT(OnBundleIdChanged(QString)));

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

    connect(ui->originalBuildBtn, SIGNAL(pressed()), this, SLOT(OnOriginalBuildClicked()));
    connect(ui->privateKeyBtn, SIGNAL(pressed()), this, SLOT(OnPrivateKeyClicked()));
    connect(ui->privateKeyEdit, SIGNAL(currentTextChanged(QString)), this, SLOT(OnPrivateKeyChanged(QString)));
    connect(ui->provisionBtn, SIGNAL(pressed()), this, SLOT(OnProvisionClicked()));
    connect(ui->codesignBtn, SIGNAL(pressed()), this, SLOT(OnCodesignClicked()));
    connect(Recodesigner::Get(), SIGNAL(SigningResult(Recodesigner::SigningStatus,QString)), this, SLOT(OnSigningResult(Recodesigner::SigningStatus,QString)));
    RefreshPrivateKeyList();

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
                   << ui->proxyBtn
                   << ui->configureBtn
                   << ui->installBtn
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
                   << ui->clearBtn
                   << ui->originalBuildBtn
                   << ui->privateKeyBtn
                   << ui->provisionBtn
                   << ui->codesignBtn
                   << ui->clearOutputBtn
                   << ui->saveOutputBtn);

    MassStylesheet(STYLE_TYPE::ROUNDED_EDIT_LIGHT, QList<QWidget*>()
                   << ui->UDID
                   << ui->maxShownLogs
                   << ui->scrollInterval
                   << ui->installPath
                   << ui->AppSigner
                   << ui->crashlogEdit
                   << ui->dsymEdit
                   << ui->searchEdit
                   << ui->excludeEdit
                   << ui->originalBuildEdit
                   << ui->privateKeyPasswordEdit
                   << ui->provisionEdit);

    MassStylesheet(STYLE_TYPE::ROUNDED_COMBOBOX_LIGHT, QList<QWidget*>()
                   << ui->bundleIds
                   << ui->socketBox
                   << ui->pidEdit
                   << ui->privateKeyEdit
                   << ui->provisionEdit);

    DecorateSplitter(ui->splitter, 1);
    DecorateSplitter(ui->topSplitter, 1);
}

MainWindow::~MainWindow()
{
    DeviceBridge::Destroy();
    CrashSymbolicator::Destroy();
    Recodesigner::Destroy();
    m_devicesModel->clear();
    delete m_devicesModel;
    delete m_table;
#if LOGVIEW_MODE == 2
    m_tableContextMenu->clear();
    delete m_tableContextMenu;
#endif
#if LOGVIEW_MODE == 1 || LOGVIEW_MODE == 2
    delete m_dataModel;
#endif
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
#if LOGVIEW_MODE == 1
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
#elif  LOGVIEW_MODE == 2
    if (!m_table)
    {
        m_dataModel = new CustomModel();
        m_dataModel->setMaxData(m_maxShownLogs);

        m_table = new QTableView();
        m_table->setModel(m_dataModel);
        m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeMode::ResizeToContents);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
        m_table->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        m_table->setHorizontalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        m_table->setWordWrap(false);
        m_table->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, SIGNAL(customContextMenuRequested(QPoint)), SLOT(OnContextMenuRequested(QPoint)));
        ui->logLayout->addWidget(m_table);

        m_tableContextMenu = new QMenu(this);
        m_tableContextMenu->addAction(new QAction("Resume", this));
        m_tableContextMenu->addSeparator();
        m_tableContextMenu->addAction(new QAction("Copy", this));
        m_tableContextMenu->addAction(new QAction("Select All", this));
        connect(m_tableContextMenu, SIGNAL(triggered(QAction*)), this, SLOT(OnContextMenuTriggered(QAction*)));
    }
#else
    if (!m_table)
    {
        m_table = new QPlainTextEdit();
        m_table->setReadOnly(true);
        m_table->setWordWrapMode(QTextOption::NoWrap);

        QFont font;
        font.setFamily("monospace [Courier New]");
        font.setFixedPitch(true);
        font.setStyleHint(QFont::Monospace);
        m_table->setFont(font);
        ui->logLayout->addWidget(m_table);
    }
#endif
}

void MainWindow::UpdateLogsFilter()
{
    m_mutex.lock();
#if LOGVIEW_MODE == 1
    m_table->clearTable();
#elif LOGVIEW_MODE == 2
    m_dataModel->clear();
#else
    m_table->clear();
    m_paddinglogs = "0|0|0|0";
#endif
    SetupLogsTable();

    QList<LogPacket> logs;
    for (LogPacket& m_Log : m_liveLogs)
    {
        if (m_Log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries)) {
            logs << m_Log;
        }
    }
    AddLogToTable(logs);
    m_mutex.unlock();
}

QList<int> MainWindow::GetPaddingLog(LogPacket log)
{
    QList<int> lengths;
    QStringList len_split = m_paddinglogs.split('|');
    for (int idx = 0; idx < len_split.size(); idx++) {
        switch (idx) {
        case 0:
            if (len_split[idx].toInt() < log.getDateTime().size())
                lengths << log.getDateTime().size();
            else
                lengths << len_split[idx].toInt();
            break;
        case 1:
            if (len_split[idx].toInt() < log.getDeviceName().size())
                lengths << log.getDeviceName().size();
            else
                lengths << len_split[idx].toInt();
            break;
        case 2:
            if (len_split[idx].toInt() < log.getProcessID().size())
                lengths << log.getProcessID().size();
            else
                lengths << len_split[idx].toInt();
            break;
        case 3:
            if (len_split[idx].toInt() < log.getLogType().size())
                lengths << log.getLogType().size();
            else
                lengths << len_split[idx].toInt();
            break;
        default:
            break;
        }
    }
    QString new_length = QString::asprintf("%d|%d|%d|%d", lengths[0], lengths[1], lengths[2], lengths[3]);
    if (!m_paddinglogs.contains(new_length)){
        m_paddinglogs = new_length;
    }
    return lengths;
}

void MainWindow::AddLogToTable(LogPacket log)
{
#if LOGVIEW_MODE == 1
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
#elif LOGVIEW_MODE == 2
    m_dataModel->addItem(log);
#else
    QList<int> lengths = GetPaddingLog(log);
    auto lines = log.getLogMessage().split('\n');
    if (lines.count() > 0)
    {
        for (qsizetype line_idx = 0; line_idx < lines.count(); line_idx++)
        {
            if (line_idx > 0)
                m_table->appendPlainText(QString("%1\t%2\t%3\t%4\t%5")
                                         .arg("", -lengths[0])
                                         .arg("", -lengths[1])
                                         .arg("", -lengths[2])
                                         .arg("", -lengths[3])
                                         .arg(lines[line_idx]));
            else
                m_table->appendPlainText(QString("%1\t%2\t%3\t%4\t%5")
                                         .arg(log.getDateTime(), -lengths[0])
                                         .arg(log.getDeviceName(), -lengths[1])
                                         .arg(log.getProcessID(), -lengths[2])
                                         .arg(log.getLogType(), -lengths[3])
                                         .arg(lines[line_idx]));
        }
    }
    else
    {
        m_table->appendPlainText(log.GetRawData());
    }
#endif
}

void MainWindow::AddLogToTable(QList<LogPacket> logs)
{
    if (logs.count() == 0)
        return;

#if LOGVIEW_MODE == 1
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
#elif LOGVIEW_MODE == 2
    m_dataModel->addItems(logs);
#else
    foreach (auto log, logs) {
        AddLogToTable(log);
    }
#endif
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
#if LOGVIEW_MODE == 1 || LOGVIEW_MODE == 2
    while ((unsigned int)m_liveLogs.size() > m_maxShownLogs)
    {
        m_liveLogs.erase(m_liveLogs.begin());
    }
#endif
    if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
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

void MainWindow::OnTextFilterChanged(QString text)
{
    m_currentFilter = text;
    UpdateLogsFilter();
}

void MainWindow::OnPidFilterChanged(QString text)
{
    QStringList userBinaries;
    if (text.trimmed().toLower().contains("by user apps only") || text.trimmed().toLower().contains("related to user apps"))
    {
        IsInstalledUpdated();
        bool isUserAppsOnly = text.trimmed().toLower().contains("by user apps only");
        QString op1 = isUserAppsOnly ? "\\[|" : "|";
        QString op2 = isUserAppsOnly ? "\\[" : "";
        foreach (auto appinfo, m_installedApps)
        {
            QString bin_name = appinfo["CFBundleExecutable"].toString();
            userBinaries << bin_name;
        }
        m_userbinaries = userBinaries.join(op1) + op2;
    }
    else
    {
        m_userbinaries.clear();
        m_pidFilter = text;
    }
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
    bool is_stop = ui->stopCheck->isChecked();
    ui->stopCheck->setChecked(true);

#if LOGVIEW_MODE == 1
    m_table->clearTable();
#elif LOGVIEW_MODE == 2
    m_dataModel->clear();
#else
    m_table->clear();
    m_paddinglogs = "0|0|0|0";
#endif
    m_liveLogs.clear();
    SetupLogsTable();

    ui->stopCheck->setChecked(is_stop);
}

void MainWindow::OnSaveClicked()
{
    bool is_stop = ui->stopCheck->isChecked();
    ui->stopCheck->setChecked(true);

#if LOGVIEW_MODE == 1
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
    m_table->clearSelectionList();
#elif LOGVIEW_MODE == 2
    QModelIndexList indexes = m_table->selectionModel()->selectedRows();
    QString data_str = "";
    if (indexes.count() <= 1)
    {
        for (qsizetype idx = 0; idx < m_dataModel->rowCount(); idx++)
            data_str.append(m_dataModel->getLogPacket(idx).GetRawData() + "\n");
    }
    else
    {
        foreach (auto item, indexes)
            data_str.append(m_dataModel->getLogPacket(item.row()).GetRawData() + "\n");
    }
    m_table->clearSelection();

    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Log", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << data_str;
            f.close();
        }
    }
#else
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Log", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << m_table->toPlainText();
            f.close();
        }
    }
#endif
    ui->stopCheck->setChecked(is_stop);
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
        RefreshPIDandBundleID();
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

void MainWindow::RefreshPIDandBundleID()
{
    if (IsInstalledUpdated() || ui->bundleIds->count() != m_installedApps.size() || (ui->pidEdit->count() - 2) != (m_installedApps.size() * 2))
    {
        ui->bundleIds->clear();
        ui->bundleIds->addItems(m_installedApps.keys());

        QString old_string = ui->pidEdit->currentText();
        ui->pidEdit->clear();
        ui->pidEdit->addItems(QStringList() << "By user apps only" << "Related to user apps");
        foreach (auto appinfo, m_installedApps)
        {
            QString bundle_id = appinfo["CFBundleExecutable"].toString();
            ui->pidEdit->addItem(bundle_id + "\\[\\d+\\]");
            ui->pidEdit->addItem(bundle_id);
        }
        ui->pidEdit->setEditText(old_string);
    }
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

void MainWindow::OnInstallClicked()
{
    m_installerLogs.clear();
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
#if LOGVIEW_MODE == 2
    m_dataModel->setMaxData(m_maxShownLogs);
#endif
    m_scrollInterval = ui->scrollInterval->text().toUInt();

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
    ui->outputEdit->appendPlainText(text);
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
    CrashSymbolicator::Get()->Process(crashpath, dsympath);
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

void MainWindow::OnProcessStatusChanged(int percentage, QString message)
{
    if (!m_loading->isActiveWindow())
        m_loading->ShowProgress("Connect to device...");

    m_loading->SetProgress(percentage, message);
    ui->statusbar->showMessage(message);
    if (percentage == 100) {
        RefreshPIDandBundleID();
        OnPidFilterChanged(ui->pidEdit->currentText());
    }
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

void MainWindow::OnBottomTabChanged(int index)
{
    static bool last_autoscroll = false;
    switch (index)
    {
    case 0:
        ui->scrollCheck->setCheckState(last_autoscroll ? Qt::Checked : Qt::Unchecked);
        break;
    default:
        last_autoscroll = ui->scrollCheck->isChecked();
        ui->scrollCheck->setCheckState(Qt::Unchecked);
        break;
    }
}

void MainWindow::OnContextMenuRequested(QPoint pos)
{
#if LOGVIEW_MODE == 2
    m_lastStopChecked = ui->stopCheck->isChecked();
    ui->stopCheck->setChecked(true);
    m_tableContextMenu->popup(m_table->viewport()->mapToGlobal(pos));
#endif
}

void MainWindow::OnContextMenuTriggered(QAction *action)
{
#if LOGVIEW_MODE == 2
    if (action->text().toLower().contains("copy"))
    {
        QModelIndexList indexes = m_table->selectionModel()->selectedRows();
        QString data_str = "";
        foreach (auto item, indexes)
            data_str.append(m_dataModel->getLogPacket(item.row()).GetRawData() + "\n");
        m_table->clearSelection();
        QApplication::clipboard()->setText(data_str);
    }
    else if (action->text().toLower().contains("select all"))
    {
        m_table->selectAll();
    }
    else if (action->text().toLower().contains("resume"))
    {
        ui->stopCheck->setChecked(false);
    }

    //ui->stopCheck->setChecked(m_tableContextMenu);
#endif
}

void MainWindow::OnClearOutputClicked()
{
    ui->outputEdit->clear();
}

void MainWindow::OnSaveOutputClicked()
{
    QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "LogOutput", this, "Text File (*.txt)");
    if (!filepath.isEmpty()) {
        QFile f(filepath);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream stream(&f);
            stream << ui->outputEdit->toPlainText();
            f.close();
        }
    }
}
