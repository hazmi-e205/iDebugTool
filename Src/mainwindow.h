#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QSplashScreen>
#include <QMutex>
#include <QPlainTextEdit>

#include "devicebridge.h"
#include "customkeyfiler.h"
#include "textviewer.h"
#include "imagemounter.h"
#include "proxydialog.h"
#include "loadingdialog.h"
#include "aboutdialog.h"
#include "recodesigner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class AppInfo;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    Ui::MainWindow *ui;
    AppInfo *m_appInfo;
    float m_ratioTopWidth;
    float m_topWidth;
    QTimer *m_scrollTimer;
    CustomKeyFilter *m_eventFilter;
    quint64 m_scrollInterval;
    TextViewer *m_textDialog;
    ProxyDialog *m_proxyDialog;
    AboutDialog *m_aboutDialog;
    QMutex m_mutex;
    bool m_lastAutoScroll;
    int m_lastMaxScroll;
    bool m_lastStopChecked;

private slots:
    void OnTopSplitterMoved(int pos, int index);
    void OnAutoScrollChecked(int state);
    void OnClickedEvent(QObject* object);
    void OnScrollTimerTick();
    void OnConfigureClicked();
    void OnProxyClicked();
    void OnUpdateClicked();
    void OnMessagesReceived(MessagesType type, QString messages);
    void OnBottomTabChanged(int index);
    void OnClearOutputClicked();
    void OnSaveOutputClicked();

    //Device UI
private:
    LoadingDialog *m_loading;
    QStandardItemModel *m_devicesModel;
    void SetupDevicesUI();
    void UpdateInfoWidget();
private slots:
    void OnDevicesTableClicked(QModelIndex index);
    void OnRefreshClicked();
    void OnUpdateDevices(QMap<QString, idevice_connection_type> devices);
    void OnDeviceConnected();
    void RefreshSocketList();
    void OnSocketClicked();
    void OnSystemInfoClicked();
    void OnProcessStatusChanged(int percentage, QString message);

    //Syslog UI
private:
    QPlainTextEdit *m_table;
    quint64 m_maxCachedLogs;
    void SetupSyslogUI();
private slots:
    void OnClearClicked();
    void OnSaveClicked();
    void OnStopChecked(int state);
    void OnSystemLogsReceived2(QString logs);
    void OnFilterStatusChanged(bool isfiltering);
    void OnTextFilterChanged(QString text);
    void OnPidFilterChanged(QString text);
    void OnExcludeFilterChanged(QString text);

    //AppManager and Installer UI
private:
    QString m_choosenBundleId;
    QMap<QString, QJsonDocument> m_installedApps;
    void SetupAppManagerUI();
    void RefreshPIDandBundleID();
private slots:
    void OnInstallClicked();
    void OnUninstallClicked();
    void OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);
    void OnBundleIdChanged(QString text);
    void OnAppInfoClicked();

    //Recodesigner UI
private:
    void SetupRecodesignerUI();
    void RefreshPrivateKeyList();
private slots:
    void OnOriginalBuildClicked();
    void OnPrivateKeyClicked();
    void OnProvisionClicked();
    void OnCodesignClicked();
    void OnPrivateKeyChanged(QString key);
    void OnSigningResult(Recodesigner::SigningStatus status, QString messages);

    //Crashlogs UI
private:
    void SetupCrashlogsUI();
private slots:
    void OnSyncCrashlogsClicked();
    void OnCrashlogsStatusChanged(QString text);
    void OnCrashlogClicked();
    void OnDsymClicked();
    void OnDwarfClicked();
    void OnSymbolicateClicked();
    void OnSymbolicateResult(QString messages, bool error);

    //Toolbox UI
private:
    ImageMounter *m_imageMounter;
    void SetupToolboxUI();
private slots:
    void OnImageMounterClicked();
    void OnScreenshotClicked();
    void OnScreenshotReceived(QString imagePath);
    void OnSleepClicked();
    void OnShutdownClicked();
    void OnRestartClicked();

    //Debugger UI
private:
    void SetupDebuggerUI();
private slots:
    void OnStartDebuggingClicked();
    void OnDebuggerClearClicked();
    void OnDebuggerSaveClicked();
    void OnDebuggerReceived(QString logs, bool stopped);
    void OnDebuggerFilterStatus(bool isfiltering);
    void OnDebuggerFilterChanged(QString text);
    void OnDebuggerExcludeChanged(QString text);
};
#endif // MAINWINDOW_H
