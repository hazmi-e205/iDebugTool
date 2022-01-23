#ifndef DEVICEBRIDGE_H
#define DEVICEBRIDGE_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <vector>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice-glue/utils.h>

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
    explicit DeviceBridge(QObject *parent = 0);
    ~DeviceBridge();

    std::vector<Device> GetDevices();
    void ConnectToDevice(Device device);
    void Init();
    void TriggerUpdateDevices();
    static DeviceBridge *Get();
    static void Destroy();

private:
    void ResetConnection();
    void UpdateDeviceInfo();
    static void DeviceEventCallback(const idevice_event_t* event, void* userdata);

    idevice_t m_device;
    lockdownd_client_t m_client;
    QJsonDocument m_deviceInfo;
    static DeviceBridge *m_instance;

signals:
     void UpdateDevices(std::vector<Device> devices);
     void DeviceInfo(QJsonDocument info);
};

#endif // DEVICEBRIDGE_H
