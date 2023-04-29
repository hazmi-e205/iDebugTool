#ifndef DEVICEBRIDGE_H
#define DEVICEBRIDGE_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <vector>
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
#include "logpacket.h"

#define TOOL_NAME                       "idebugtool"
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

class DeviceBridge : public QObject
{
    Q_OBJECT
public:
    DeviceBridge();
    ~DeviceBridge();

    void Init(QWidget *parent);

    void ConnectToDevice(QString udid);
    QString GetCurrentUdid();
    bool IsConnected();
    QJsonDocument GetDeviceInfo();
    void ResetConnection();
    QMap<QString, idevice_connection_type> GetDevices();
    void StartDiagnostics(DiagnosticsMode mode);
    QStringList GetMountedImages();
    bool IsImageMounted();
    void MountImage(QString image_path, QString signature_path);
    void Screenshot(QString path);

    static DeviceBridge *Get();
    static void Destroy();

private:
    void UpdateDeviceInfo();
    void StartServices();
    void StartLockdown(bool condition, QStringList service_ids, const std::function<void(QString& service_id, lockdownd_service_descriptor_t& service)>& function);
    void TriggerUpdateDevices(idevice_event_type eventType, idevice_connection_type connectionType, QString udid);

    static void DeviceEventCallback(const idevice_event_t* event, void* userdata);
    static ssize_t ImageMounterCallback(void* buf, size_t size, void* userdata);

    idevice_t m_device;
    lockdownd_client_t m_client;
    diagnostics_relay_client_t m_diagnostics;
    mobile_image_mounter_client_t m_imageMounter;
    screenshotr_client_t m_screenshot;
    QMap<QString, QJsonDocument> m_deviceInfo;
    QMap<QString, idevice_connection_type> m_deviceList;
    QString m_currentUdid;

    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(QMap<QString, idevice_connection_type> devices);
     void DeviceConnected();
     void ProcessStatusChanged(int percentage, QString message);
     void MounterStatusChanged(QString messages);
     void ScreenshotReceived(QString imagepath);
     void MessagesReceived(MessagesType type, QString messages);

     //AFCUtils
 public:
     void SyncCrashlogs(QString path);
 private:
     int afc_upload_file(afc_client_t &afc, const QString &filename, const QString &dstfn, std::function<void(uint32_t,uint32_t)> callback = nullptr);
     bool afc_upload_dir(afc_client_t &afc, const QString &path, const QString &afcpath, std::function<void(int,int,QString)> callback = nullptr);
     int afc_copy_crash_reports(afc_client_t &afc, const char* device_directory, const char* host_directory, const char* target_dir = nullptr, const char* filename_filter = nullptr);
     afc_client_t m_afc;
     afc_client_t m_crashlog;
     QString m_crashlogTargetDir;
 signals:
     void CrashlogsStatusChanged(QString messages);

     //InstallerBridge
 public:
     QJsonDocument GetInstalledApps();
     QMap<QString, QJsonDocument> GetInstalledApps(bool doAsync);
     void UninstallApp(QString bundleId);
     void InstallApp(InstallerMode cmd, QString path);
 private:
     static void InstallerCallback(plist_t command, plist_t status, void *unused);
     void TriggetInstallerStatus(QJsonDocument command, QJsonDocument status);
     instproxy_client_t m_installer;
     QMap<QString, QJsonDocument> m_installedApps;
 signals:
     void InstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);

     //SyslogBridge
 public:
 private:
     static void SystemLogsCallback(char c, void *user_data);
     void TriggerSystemLogsReceived(LogPacket log);
     syslog_relay_client_t m_syslog;
 signals:
     void SystemLogsReceived(LogPacket log);
};

#endif // DEVICEBRIDGE_H
