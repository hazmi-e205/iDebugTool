#ifndef DEVICEBRIDGE_H
#define DEVICEBRIDGE_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice-glue/utils.h>
#include <libimobiledevice/syslog_relay.h>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/mobile_image_mounter.h>
#include <libimobiledevice/screenshotr.h>
#include <libimobiledevice/service.h>
#include <libimobiledevice/debugserver.h>
#include <libimobiledevice/house_arrest.h>
#include "debuggerfilterthread.h"
#include "logpacket.h"
#include "logfilterthread.h"
#include "utils.h"
#include "deviceclient.h"
#include "asyncmanager.h"

#include "idevice/instrument/dtxchannel.h"
#include "idevice/instrument/dtxconnection.h"
#include "idevice/instrument/dtxtransport.h"
using namespace idevice;

#define ITUNES_METADATA_PLIST_FILENAME  "iTunesMetadata.plist"
#define PKG_PATH                        "PublicStaging"
#define APPARCH_PATH                    "ApplicationArchives"
#define PATH_PREFIX                     "/private/var/mobile/Media"

enum InstallerMode {
    CMD_INSTALL,
    CMD_UPGRADE,
    CMD_UNINSTALL
};

enum DiagnosticsMode {
    CMD_SLEEP,
    CMD_RESTART,
    CMD_SHUTDOWN
};

enum MounterType {
    DISK_IMAGE_UPLOAD_TYPE_AFC,
    DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE
};

enum MessagesType {
    MSG_INFO,
    MSG_ERROR,
    MSG_WARN
};

enum GenericStatus {
    SUCCESS,
    FAILED,
    IN_PROGRESS
};

enum FileOperation {
    FETCH,
    PUSH,
    PULL,
    RENAME,
    DELETE_OP,
    MAKE_FOLDER
};

enum ConnectionStatus {
    DISCONNECTED,
    CONNECTED
};

enum MobileOperation {
    DEVICE_INFO,
    DIAGNOSTICS,
    SCREENSHOOT,
    GET_MOUNTED,
    MOUNT_IMAGE,
    CRASHLOG,
    FILE_LIST,
    DELETE_FILE,
    RENAME_FILE,
    NEW_FOLDER,
    PUSH_FILE,
    PULL_FILE,
    GET_APPS,
    INSTALL_APP,
    UNINSTALL_APP,
    SYSLOG,
    DEBUGGER
};

class DeviceBridge : public QObject
{
    Q_OBJECT
public:
    DeviceBridge();
    ~DeviceBridge();

    void Init(QWidget *parent);
    void ConnectToDevice(QString udid);
    void ConnectToDevice(QString ipAddress, int port);
    QString GetCurrentUdid();
    bool IsConnected();
    QJsonDocument GetDeviceInfo(QString udid = "");
    void ResetConnection();
    QMap<QString, idevice_connection_type> GetDevices();

    static DeviceBridge *Get();
    static void Destroy();

private:
    void CreateClient(MobileOperation operation, QStringList service_ids = QStringList(), QStringList service_ids_2 = QStringList());
    void RemoveClient(MobileOperation operation);
    bool IsClientOk(MobileOperation operation);
    lockdownd_service_descriptor_t GetService(MobileOperation operation,  QStringList service_ids);
    void ConnectToDevice(const std::function<void()>& configureConnection);

    void UpdateDeviceInfo();
    void StartLockdown(bool condition, lockdownd_client_t& client, QStringList service_ids, const std::function<void(QString& service_id, lockdownd_service_descriptor_t& service)>& function, bool clear_lockdownd = true);
    void TriggerUpdateDevices(idevice_event_type eventType, idevice_connection_type connectionType, QString udid);

    static void DeviceEventCallback(const idevice_event_t* event, void* userdata);
    static bool m_destroyed;

    idevice_t m_device;
    QMap<MobileOperation,DeviceClient*> m_clients;
    QMap<QString, QJsonDocument> m_deviceInfo;
    QMap<QString, idevice_connection_type> m_deviceList;
    QString m_currentUdid;
    bool m_isRemote;
    QMutex m_mutex;
    RemoteAddress m_remoteAddress;

    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(QMap<QString, idevice_connection_type> devices);
     void DeviceStatus(ConnectionStatus status, QString udid, bool isRemote);
     void ProcessStatusChanged(int percentage, QString message);
     void MessagesReceived(MessagesType type, QString messages);

     //Misc
 public:
    void StartDiagnostics(DiagnosticsMode mode);
    void Screenshot(QString path);
 signals:
     void ScreenshotReceived(QString imagepath);

     //AFCUtils
 private:
     int afc_upload_file(afc_client_t &afc, const QString &filename, const QString &dstfn, std::function<void(uint32_t,uint32_t)> callback = nullptr);
     bool afc_upload_dir(afc_client_t &afc, const QString &path, const QString &afcpath, std::function<void(int,int,QString)> callback = nullptr);
     int afc_copy_crash_reports(afc_client_t &afc, const char* device_directory, const char* host_directory, const char* target_dir = nullptr, const char* filename_filter = nullptr);
     void afc_traverse_recursive(afc_client_t afc, const char* path);

     //Mounter
 public:
    QStringList GetMountedImages();
    bool IsImageMounted();
    void MountImage(QString image_path, QString signature_path);
 private:
    void mount_image(QString image_path, QString signature_path);
    static ssize_t ImageMounterCallback(void* buf, size_t size, void* userdata);
 signals:
     void MounterStatusChanged(QString messages);

     //Crashlog
 public:
     void SyncCrashlogs(QString path);
 private:
     lockdownd_client_t m_crashlogClient;
     afc_client_t m_crashlog;
     QString m_crashlogTargetDir;
 signals:
     void CrashlogsStatusChanged(QString messages);

     //FileManager
 public:
     struct FileProperty {
         bool isDirectory = false;
         quint64 sizeInBytes = 0;
     };
     void GetAccessibleStorage(QString startPath = "/", QString bundleId = "");
     void PushToStorage(QString localPath, QString devicePath, QString bundleId = "");
     void PullFromStorage(QString devicePath, QString localPath, QString bundleId = "");
     void DeleteFromStorage(QString devicePath, QString bundleId = "");
     void MakeDirectoryToStorage(QString devicePath, QString bundleId = "");
     void RenameToStorage(QString oldPath, QString newPath, QString bundleId = "");
 private:
     void afc_filemanager_action(std::function<void(afc_client_t &afc)> action, const QString& bundleId = "");
     lockdownd_client_t m_fileClient;
     afc_client_t m_fileManager;
     house_arrest_client_t m_houseArrest;
     QMap<QString, FileProperty> m_accessibleStorage;
 signals:
     void AccessibleStorageReceived(QMap<QString, FileProperty> contents);
     void FileManagerChanged(GenericStatus status, FileOperation operation, int percentage, QString message);

     //InstallerBridge
 public:
     QJsonDocument GetInstalledApps();
     QMap<QString, QJsonDocument> GetInstalledApps(bool doAsync);
     void UninstallApp(QString bundleId);
     void InstallApp(InstallerMode cmd, QString path);
 private:
     void install_app(afc_client_t &afc, InstallerMode cmd, QString path);
     static void InstallerCallback(plist_t command, plist_t status, void *unused);
     void TriggetInstallerStatus(QJsonDocument command, QJsonDocument status);
     void installer_action(std::function<void()> action);
     instproxy_client_t m_installer;
     afc_client_t m_buildSender;
     lockdownd_client_t m_installerClient;
     QMap<QString, QJsonDocument> m_installedApps;
 signals:
     void InstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);

     //SyslogBridge
 public:
     void StartSyslog();
     void StopSyslog();
     void ClearCachedLogs();
     void CaptureSystemLogs(bool enable);
     bool IsSystemLogsCaptured();
     void SetMaxCachedLogs(qsizetype number);
     void LogsFilterByString(QString text_or_regex);
     void LogsExcludeByString(QString exclude_text);
     void LogsFilterByPID(QString pid_name);
     void SystemLogsFilter(QString text_or_regex, QString pid_name, QString exclude_text);
     void ReloadLogsFilter();
     LogFilterThread* GetLogHandler() { return m_logHandler; }
     static QStringList GetPIDOptions(QMap<QString, QJsonDocument>& installed_apps);
 private:
     static void SystemLogsCallback(char c, void *user_data);
     void TriggerSystemLogsReceived(LogPacket log);
     lockdownd_client_t m_syslogClient;
     syslog_relay_client_t m_syslog;
     LogFilterThread* m_logHandler;
 signals:
     void FilterStatusChanged(bool isfiltering);
     void SystemLogsReceived(LogPacket log);
     void SystemLogsReceived2(QString logs);

     //DebugBridge
public:
     void StartDebugging(QString bundleId, bool detach_after_start = false, QString parameters = "", QString arguments = "");
     void StopDebugging();
     void ClearDebugger();
     void SetMaxDebuggerLogs(qsizetype number);
     void DebuggerFilterByString(QString text_or_regex);
     void DebuggerExcludeByString(QString exclude_text);
     void DebuggerFilter(QString text_or_regex, QString exclude_text);
     void DebuggerReloadFilter();
private:
     void CloseDebugger();
     debugserver_error_t DebugServerHandleResponse(debugserver_client_t client, char** response, int* exit_status);
     lockdownd_client_t m_debugClient;
     debugserver_client_t m_debugger;
     DebuggerFilterThread *m_debugHandler;
signals:
     void DebuggerReceived(QString messages, bool stopped = false);
     void DebuggerFilterStatus(bool isfiltering);

     // Instruments
 public:
     enum class AttrType {
         SYSTEM,
         PROCESS
     };
     QStringList GetAttributes(AttrType type);
     void StartMonitor(unsigned int interval_ms, QStringList system_attr, QStringList process_attr);
     void StopMonitor();
     void GetProcessList();
     void StartFPS(unsigned int interval_ms);
     void StopFPS();
 private:
     DTXTransport* m_transport;
     DTXConnection* m_connection;
     std::shared_ptr<DTXChannel> m_sysmontapChannel, m_openglChannel;
 signals:
};

#endif // DEVICEBRIDGE_H
