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
    void SetupDevicesTable();
    void SetupLogsTable();
    void UpdateInfoWidget();
    void RefreshSocketList();
    bool IsInstalledUpdated();
    void RefreshPIDandBundleID();
    void RefreshPrivateKeyList();
    Ui::MainWindow *ui;
    QStandardItemModel *m_devicesModel;
    AppInfo *m_appInfo;
    float m_ratioTopWidth;
    float m_topWidth;
    QString m_choosenBundleId;
    QString m_installerLogs;
    std::vector<LogPacket> m_liveLogs;
    QMap<QString, QJsonDocument> m_installedApps;
    QTimer *m_scrollTimer;
    CustomKeyFilter *m_eventFilter;
    quint64 m_maxShownLogs, m_scrollInterval;
    TextViewer *m_textDialog;
    ImageMounter *m_imageMounter;
    ProxyDialog *m_proxyDialog;
    AboutDialog *m_aboutDialog;
    LoadingDialog *m_loading;
    QPlainTextEdit *m_table;
    QMutex m_mutex;
    bool m_lastAutoScroll;
    int m_lastMaxScroll;
    bool m_lastStopChecked;
    QMenu *m_tableContextMenu;

private slots:
    void OnTopSplitterMoved(int pos, int index);
    void OnDevicesTableClicked(QModelIndex index);
    void OnRefreshClicked();
    void OnUpdateDevices(QMap<QString, idevice_connection_type> devices);
    void OnDeviceConnected();
    void OnSystemLogsReceived2(QString logs);
    void OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);
    void OnTextFilterChanged(QString text);
    void OnPidFilterChanged(QString text);
    void OnExcludeFilterChanged(QString text);
    void OnAutoScrollChecked(int state);
    void OnStopChecked(int state);
    void OnClearClicked();
    void OnSaveClicked();
    void OnClickedEvent(QObject* object);
    void OnInstallClicked();
    void OnUninstallClicked();
    void OnScrollTimerTick();
    void OnConfigureClicked();
    void OnProxyClicked();
    void OnSleepClicked();
    void OnShutdownClicked();
    void OnRestartClicked();
    void OnBundleIdChanged(QString text);
    void OnSystemInfoClicked();
    void OnAppInfoClicked();
    void OnImageMounterClicked();
    void OnScreenshotClicked();
    void OnSocketClicked();
    void OnSyncCrashlogsClicked();
    void OnCrashlogsStatusChanged(QString text);
    void OnCrashlogClicked();
    void OnDsymClicked();
    void OnDwarfClicked();
    void OnSymbolicateClicked();
    void OnSymbolicateResult(QString messages, bool error);
    void OnScreenshotReceived(QString imagePath);
    void OnProcessStatusChanged(int percentage, QString message);
    void OnUpdateClicked();
    void OnMessagesReceived(MessagesType type, QString messages);
    void OnSigningResult(Recodesigner::SigningStatus status, QString messages);
    void OnOriginalBuildClicked();
    void OnPrivateKeyClicked();
    void OnProvisionClicked();
    void OnCodesignClicked();
    void OnPrivateKeyChanged(QString key);
    void OnBottomTabChanged(int index);
    void OnClearOutputClicked();
    void OnSaveOutputClicked();
};
#endif // MAINWINDOW_H
