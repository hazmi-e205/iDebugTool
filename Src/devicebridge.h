#ifndef DEVICEBRIDGE_H
#define DEVICEBRIDGE_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <vector>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice-glue/utils.h>
#include <libimobiledevice/syslog_relay.h>
#include "logpacket.h"

#define TOOL_NAME "idebugtool"

struct Device
{
    Device() {}
    Device(QString udid, idevice_connection_type type) : udid(udid), type(type) {}
    QString udid;
    idevice_connection_type type;
};

class DeviceBridge : public QObject
{
    Q_OBJECT
public:
    DeviceBridge();
    ~DeviceBridge();

    std::vector<Device> GetDevices();
    void ConnectToDevice(Device device);
    void Init(QWidget *parent);
    static DeviceBridge *Get();
    static void Destroy();

private:
    void ResetConnection();
    void UpdateDeviceInfo();
    void StartSystemLogs();
    void TriggerUpdateDevices();
    void TriggerSystemLogsReceived(LogPacket log);
    static void DeviceEventCallback(const idevice_event_t* event, void* userdata);
    static void SystemLogsCallback(char c, void *user_data);

    idevice_t m_device;
    lockdownd_client_t m_client;
    syslog_relay_client_t m_syslog;
    QJsonDocument m_deviceInfo;
    QWidget *m_mainWidget;
    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(std::vector<Device> devices);
     void DeviceInfo(QJsonDocument info);
     void SystemLogsReceived(LogPacket log);
};

#endif // DEVICEBRIDGE_H
