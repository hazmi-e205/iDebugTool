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
#include "logpacket.h"

#define TOOL_NAME                       "idebugtool"
#define ITUNES_METADATA_PLIST_FILENAME  "iTunesMetadata.plist"
#define PKG_PATH                        "PublicStaging"
#define APPARCH_PATH                    "ApplicationArchives"
#define PATH_PREFIX                     "/private/var/mobile/Media"

enum InstallMode {
    CMD_INSTALL,
    CMD_UPGRADE
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

    void ConnectToDevice(QString udid, idevice_connection_type type);
    void ResetConnection();
    std::map<QString, idevice_connection_type> GetDevices();
    void StartDiagnostics(DiagnosticsMode mode);
    QJsonDocument GetInstalledApps();
    void UninstallApp(QString bundleId);
    void InstallApp(InstallMode cmd, QString path);
    QJsonDocument GetMountedImages();
    void MountImage(QString image_path, QString signature_path);

    static DeviceBridge *Get();
    static void Destroy();

private:
    void UpdateDeviceInfo();
    void StartServices();
    void StartLockdown(bool condition, QStringList service_ids, const std::function<void(QString& service_id, lockdownd_service_descriptor_t& service)>& function);
    void TriggerUpdateDevices();
    void TriggerSystemLogsReceived(LogPacket log);

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
    QJsonDocument m_deviceInfo;
    QWidget *m_mainWidget;

    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(std::map<QString, idevice_connection_type> devices);
     void DeviceInfoReceived(QJsonDocument info);
     void SystemLogsReceived(LogPacket log);
};

#endif // DEVICEBRIDGE_H
