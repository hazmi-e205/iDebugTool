#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include "utils.h"
#include "appinfo.h"
#include "userconfigs.h"
#include "crashsymbolicator.h"
#include "asyncmanager.h"
#include <QFile>
#include <QMimeData>
#include <QScrollBar>
#include <QMessageBox>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_appInfo(new AppInfo(this))
    , m_ratioTopWidth(0.4f)
    , m_scrollTimer(new QTimer(this))
    , m_eventFilter(new CustomKeyFilter())
    , m_scrollInterval(UserConfigs::Get()->GetData("ScrollInterval", "250").toUInt())
    , m_textDialog(new TextViewer(this))
    , m_proxyDialog(new ProxyDialog(this))
    , m_aboutDialog(new AboutDialog(&m_appInfo, this))
    , m_loading(new LoadingDialog(this))
    , m_devicesModel(nullptr)
    , m_table(new QPlainTextEdit())
    , m_maxCachedLogs(UserConfigs::Get()->GetData("MaxShownLogs", "1000").toUInt())
    , m_imageMounter(new ImageMounter(this))
{
    ui->setupUi(this);

    AsyncManager::Get()->Init(4);
    QMainWindow::setWindowTitle(m_appInfo->GetFullname());
    QMainWindow::setWindowIcon(QIcon(":res/bulb.ico"));
    DeviceBridge::Get()->Init(this);
    connect(DeviceBridge::Get(), SIGNAL(MessagesReceived(MessagesType,QString)), this, SLOT(OnMessagesReceived(MessagesType,QString)));
    connect(ui->topSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnTopSplitterMoved(int,int)));
    connect(ui->scrollCheck, SIGNAL(stateChanged(int)), this, SLOT(OnAutoScrollChecked(int)));
    connect(ui->saveOutputBtn, SIGNAL(pressed()), this, SLOT(OnSaveOutputClicked()));
    connect(ui->clearOutputBtn, SIGNAL(pressed()), this, SLOT(OnClearOutputClicked()));
    connect(ui->updateBtn, SIGNAL(pressed()), this, SLOT(OnUpdateClicked()));
    connect(ui->bottomWidget, SIGNAL(currentChanged(int)), this, SLOT(OnBottomTabChanged(int)));
    connect(ui->aboutBtn, SIGNAL(pressed()), m_aboutDialog, SLOT(show()));
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(OnScrollTimerTick()));
    if (ui->scrollCheck->isChecked())
        m_scrollTimer->start(m_scrollInterval);

    ui->statusbar->showMessage("Idle");
    SetupDevicesUI();
    SetupSyslogUI();
    SetupAppManagerUI();
    SetupRecodesignerUI();
    SetupCrashlogsUI();
    SetupToolboxUI();

    ui->installDrop->installEventFilter(m_eventFilter);
    ui->bundleIds->installEventFilter(m_eventFilter);
    ui->pidEdit->installEventFilter(m_eventFilter);
    connect(m_eventFilter, SIGNAL(pressed(QObject*)), this, SLOT(OnClickedEvent(QObject*)));

    ui->scrollInterval->setText(QString::number(m_scrollInterval));
    connect(ui->configureBtn, SIGNAL(pressed()), this, SLOT(OnConfigureClicked()));
    connect(ui->proxyBtn, SIGNAL(pressed()), this, SLOT(OnProxyClicked()));
    m_proxyDialog->UseExisting();

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

        // Drop in Dashboard, we will install it...
        if (ui->featureWidget->currentIndex() == 0 && (fileName.endsWith(".ipa", Qt::CaseInsensitive) || fileName.endsWith(".app", Qt::CaseInsensitive)))
        {
            ui->installPath->setText(fileName);
            OnInstallClicked();
            UserConfigs::Get()->SaveData("LastAppDir", GetBaseDirectory(fileName));
            break;
        }
    }
}

void MainWindow::OnTopSplitterMoved(int pos, int index)
{
    m_ratioTopWidth = (float)ui->deviceWidget->width() / m_topWidth;
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
    m_maxCachedLogs = ui->maxShownLogs->text().toUInt();
    DeviceBridge::Get()->SetMaxCachedLogs(m_maxCachedLogs);
    m_scrollInterval = ui->scrollInterval->text().toUInt();

    UserConfigs::Get()->SaveData("MaxShownLogs", ui->maxShownLogs->text());
    UserConfigs::Get()->SaveData("ScrollInterval", ui->scrollInterval->text());
}

void MainWindow::OnProxyClicked()
{
    m_proxyDialog->ShowDialog();
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
