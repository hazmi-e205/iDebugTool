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

class DeviceBridge : public QObject
{
    Q_OBJECT
public:
    DeviceBridge();
    ~DeviceBridge();

    void Init(QWidget *parent);
    QWidget *GetMainWidget() { return m_mainWidget; }

    void ConnectToDevice(QString udid);
    QString GetCurrentUdid();
    bool IsConnected();
    QJsonDocument GetDeviceInfo();
    void ResetConnection();
    QMap<QString, idevice_connection_type> GetDevices();
    void StartDiagnostics(DiagnosticsMode mode);
    QJsonDocument GetInstalledApps();
    void UninstallApp(QString bundleId);
    void InstallApp(InstallerMode cmd, QString path);
    QStringList GetMountedImages();
    void MountImage(QString image_path, QString signature_path);
    bool Screenshot(QString path);

    static DeviceBridge *Get();
    static void Destroy();

private:
    void UpdateDeviceInfo();
    void StartServices();
    void StartLockdown(bool condition, QStringList service_ids, const std::function<void(QString& service_id, lockdownd_service_descriptor_t& service)>& function);
    void TriggerUpdateDevices(idevice_event_type eventType, idevice_connection_type connectionType, QString udid);
    void TriggerSystemLogsReceived(LogPacket log);
    void TriggetInstallerStatus(QJsonDocument command, QJsonDocument status);

    static void DeviceEventCallback(const idevice_event_t* event, void* userdata);
    static void SystemLogsCallback(char c, void *user_data);
    static void InstallerCallback(plist_t command, plist_t status, void *unused);
    static ssize_t ImageMounterCallback(void* buf, size_t size, void* userdata);

    idevice_t m_device;
    lockdownd_client_t m_client;
    syslog_relay_client_t m_syslog;
    instproxy_client_t m_installer;
    afc_client_t m_afc;
    diagnostics_relay_client_t m_diagnostics;
    mobile_image_mounter_client_t m_imageMounter;
    screenshotr_client_t m_screenshot;
    QWidget *m_mainWidget;
    QMap<QString, QJsonDocument> m_deviceInfo;
    QMap<QString, idevice_connection_type> m_deviceList;
    QString m_currentUdid;

    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(QMap<QString, idevice_connection_type> devices);
     void DeviceConnected();
     void SystemLogsReceived(LogPacket log);
     void InstallerStatusChanged(InstallerMode command, QString bundleId, int percentage, QString message);
     void MounterStatusChanged(QString messages);
};

#endif // DEVICEBRIDGE_H
