#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QSplashScreen>
#include <QMutex>
#include <QPlainTextEdit>
#include <QHeaderView>

#include "crashsymbolicator.h"
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
class AppleDB;
class QLabel;
class QItemSelection;
class QWidget;
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
    AppleDB* m_appleDb;

private slots:
    void OnTopSplitterMoved(int pos, int index);
    void OnAutoScrollChecked(int state);
    void OnClickedEvent(QObject* object);
    void OnScrollTimerTick();
    void OnOutputSliderMoved(int value);
    void OnConfigureClicked();
    void OnProxyClicked();
    void OnUpdateClicked();
    void OnMessagesReceived(MessagesType type, QString messages);
    void OnClearOutputClicked();
    void OnSaveOutputClicked();

    //Device UI
private:
    LoadingDialog *m_loadingDevice;
    QStandardItemModel *m_devicesModel;
    void SetupDevicesUI();
    void UpdateInfoWidget();
private slots:
    void OnDevicesTableClicked(QModelIndex index);
    void OnRefreshClicked();
    void OnUpdateDevices(QMap<QString, idevice_connection_type> devices);
    void OnDeviceStatus(ConnectionStatus status, QString udid, bool isRemote);
    void RefreshSocketList();
    void OnSocketClicked();
    void OnSystemInfoClicked();
    void OnProcessStatusChanged(int percentage, QString message);

    //Syslog UI
private:
    quint64 m_maxCachedLogs;
    void SetupSyslogUI();
private slots:
    void OnSyslogSliderMoved(int value);
    void OnClearClicked();
    void OnSaveClicked();
    void OnStartLogging();
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
    void RefreshPIDandBundleID(bool async = true);
private slots:
    void OnInstallClicked();
    void OnUninstallClicked();
    void OnInstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);
    void OnBundleIdChanged(QString text);
    void OnAppInfoClicked();

    //Recodesigner UI
private:
    LoadingDialog *m_loadingCodesign;
    void SetupRecodesignerUI();
    void RefreshPrivateKeyList();
private slots:
    void OnOriginalBuildClicked();
    void OnPrivateKeyClicked();
    void OnProvisionClicked();
    void OnProvisionExt1Clicked();
    void OnProvisionExt2Clicked();
    void OnEntitlementClicked();
    void OnCodesignClicked();
    void OnPrivateKeyChanged(QString key);
    void OnSigningResult(Recodesigner::SigningStatus status, float percentage, QString messages);

    //Crashlogs UI
private:
    LoadingDialog *m_loadingSymbolicate;
    QStandardItemModel *m_stacktraceModel;
    SymbolicatedData m_lastStacktrace;
    void SetupCrashlogsUI();
private slots:
    void OnSyncCrashlogsClicked();
    void OnCrashlogsDirClicked();
    void OnCrashlogsStatusChanged(QString text);
    void OnCrashlogClicked();
    void OnDsymClicked();
    void OnDwarfClicked();
    void OnSymbolicateClicked();
    void OnSaveSymbolicatedClicked();
    void OnStacktraceThreadChanged(QString threadName);
    void OnSymbolicateResult2(unsigned int progress, SymbolicatedData data, bool error);

    //FileManager UI
private:
    QStandardItemModel *m_fileManagerModel;
    LoadingDialog *m_loadingFileOperation;
    QMap<QString, DeviceBridge::FileProperty> m_cachedFiles;
    int m_fileManagerNameWidth;
    int m_fileManagerSizeWidth;
    QString m_currentFileManagerBundleId;
    QMap<QString, QSet<QString>> m_expandedPathsByBundle;
    QMap<QString, QSet<QString>> m_loadedFileManagerDirsByBundle;
    QWidget *m_fileDetailsPanel;
    QLabel *m_filePreviewLabel;
    QLabel *m_fileDetailNameLabel;
    QLabel *m_fileDetailPathLabel;
    QLabel *m_fileDetailTypeLabel;
    QLabel *m_fileDetailSizeLabel;
    QLabel *m_fileDetailChildrenLabel;
    QString m_pendingPreviewPath;
    QString m_currentPreviewLocalPath;
    void SetupFileManagerUI();
    void FileManagerAction(std::function<void(QString&,QString&)> action, bool asFolder = true);
    void ResetFileBrowser();
    void CacheHeaderSizes(QHeaderView *header, int &nameWidth, int &sizeWidth);
    void RestoreHeaderSizes(QHeaderView *header, int nameWidth, int sizeWidth);
    void SaveExpandedPaths(const QString& bundleId);
    void RestoreExpandedPaths(const QString& bundleId);
    void RequestFileManagerDirectoryFetch(const QString& directoryPath, bool partialUpdate = true);
    QStandardItem* FindFileManagerItemByPath(const QString& path);
    void PopulateFileManagerDirectory(const QString& directoryPath);
    void AddDirectoryPlaceholderIfNeeded(QStandardItem* directoryItem, const DeviceBridge::FileProperty& prop);
    void SetupFileDetailsPanel();
    void UpdateFileDetailsPanel(const QString& path);
    void ClearFilePreview(const QString& message = QString());
    void UpdateFilePreview(const QString& devicePath, const QString& localPath);
private slots:
    void OnStorageChanged(QString storage);
    void OnAccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty> contents, QString startPath, bool partialUpdate);
    void OnFileManagerChanged(GenericStatus status, FileOperation operation, int percentage, QString message);
    void OnRefreshFileBrowserClicked();
    void OnFileBrowserExpanded(const QModelIndex& index);
    void OnPullFileClicked();
    void OnPushFileClicked();
    void OnDeleteFileClicked();
    void OnRenameFileClicked();
    void OnMakeFolderClicked();
    void OnFileFilterChanged(QString filter);
    void OnFileBrowserSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void OnFilePreviewReceived(QString devicePath, QString localPath, bool success, QString errorMessage);

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
    void OnDebuggerSliderMoved(int value);
    void OnDebuggerClearClicked();
    void OnDebuggerSaveClicked();
    void OnDebuggerReceived(QString logs, bool stopped);
    void OnDebuggerFilterStatus(bool isfiltering);
    void OnDebuggerFilterChanged(QString text);
    void OnDebuggerExcludeChanged(QString text);

    //Instrument UI
private:
    void SetupInstrumentUI();
private slots:
    void OnInstrumentLoadSystemAttrsClicked();
    void OnInstrumentLoadProcessAttrsClicked();
    void OnInstrumentStartStopMonitorClicked();
    void OnInstrumentGetProcessListClicked();
    void OnInstrumentLaunchAppClicked();
    void OnInstrumentKillAppClicked();
    void OnInstrumentStartStopFpsClicked();
    void OnInstrumentClearLogClicked();
    void OnInstrumentLogReceived(QString logs);
};
#endif // MAINWINDOW_H
