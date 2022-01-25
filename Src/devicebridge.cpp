#include "devicebridge.h"
#include "utils.h"
#include <QDebug>
#include <QMessageBox>

DeviceBridge *DeviceBridge::m_instance = nullptr;
DeviceBridge *DeviceBridge::Get()
{
    if(!m_instance)
        m_instance = new DeviceBridge();
    return m_instance;
}

void DeviceBridge::Destroy()
{
    if(m_instance)
        delete m_instance;
}

DeviceBridge::DeviceBridge() :
    m_device(nullptr),
    m_client(nullptr),
    m_syslog(nullptr),
    m_mainWidget(nullptr)
{
}

DeviceBridge::~DeviceBridge()
{
    idevice_event_unsubscribe();
    ResetConnection();
}

void DeviceBridge::Init(QWidget *parent)
{
    m_mainWidget = parent;
    idevice_event_subscribe(DeviceEventCallback, nullptr);
}

std::vector<Device> DeviceBridge::GetDevices()
{
    std::vector<Device> pDevices;
    idevice_info_t *dev_list = NULL;
    int i;
    if (idevice_get_device_list_extended(&dev_list, &i) < 0)
    {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Unable to retrieve device list!", QMessageBox::Ok);
    }
    else
    {
        for (i = 0; dev_list[i] != NULL; i++)
        {
            pDevices.push_back(Device(dev_list[i]->udid, dev_list[i]->conn_type));
        }
        idevice_device_list_extended_free(dev_list);
    }
    return pDevices;
}

void DeviceBridge::ResetConnection()
{
    if (m_syslog)
    {
        syslog_relay_client_free(m_syslog);
        m_syslog = nullptr;
    }

    if(m_client)
    {
        lockdownd_client_free(m_client);
        m_client = nullptr;
    }

    if(m_device)
    {
        idevice_free(m_device);
        m_device = nullptr;
    }
}

void DeviceBridge::ConnectToDevice(Device device)
{
    ResetConnection();

    //connect to udid
    idevice_new_with_options(&m_device, device.udid.toStdString().c_str(), device.type == CONNECTION_USBMUXD ? IDEVICE_LOOKUP_USBMUX : IDEVICE_LOOKUP_NETWORK);
    if (!m_device) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: No device with UDID " + device.udid, QMessageBox::Ok);
        return;
    }
    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(m_device, &m_client, TOOL_NAME)) {
        idevice_free(m_device);
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Connecting to " + device.udid + " failed!", QMessageBox::Ok);
        return;
    }

    UpdateDeviceInfo();
    StartSystemLogs();
}

void DeviceBridge::UpdateDeviceInfo()
{
    plist_t node = nullptr;
    if(lockdownd_get_value(m_client, nullptr, nullptr, &node) == LOCKDOWN_E_SUCCESS) {
        if (node) {
            m_deviceInfo = PlistToJson(node);
            plist_free(node);
            node = nullptr;
            emit DeviceInfo(m_deviceInfo);
        }
    }
}

void DeviceBridge::StartSystemLogs()
{
    /* start syslog_relay service */
    lockdownd_service_descriptor_t svc = nullptr;
    lockdownd_error_t lerr = lockdownd_start_service(m_client, SYSLOG_RELAY_SERVICE_NAME, &svc);
    if (lerr == LOCKDOWN_E_PASSWORD_PROTECTED) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Device is passcode protected, enter passcode on the device to continue.", QMessageBox::Ok);
        return;
    }
    if (lerr != LOCKDOWN_E_SUCCESS) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to lockdownd: " + QString::number(lerr), QMessageBox::Ok);
        return;
    }

    /* connect to syslog_relay service */
    syslog_relay_error_t serr = SYSLOG_RELAY_E_UNKNOWN_ERROR;
    serr = syslog_relay_client_new(m_device, svc, &m_syslog);
    lockdownd_service_descriptor_free(svc);
    if (serr != SYSLOG_RELAY_E_SUCCESS) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not start service com.apple.syslog_relay.", QMessageBox::Ok);
        return;
    }

    /* start capturing syslog */
    serr = syslog_relay_start_capture_raw(m_syslog, SystemLogsCallback, nullptr);
    if (serr != SYSLOG_RELAY_E_SUCCESS) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Unable to start capturing syslog.", QMessageBox::Ok);
        syslog_relay_client_free(m_syslog);
        m_syslog = nullptr;
        return;
    }
}

void DeviceBridge::TriggerUpdateDevices()
{
    emit UpdateDevices(GetDevices());
}

void DeviceBridge::TriggerSystemLogsReceived(LogPacket log)
{
    emit SystemLogsReceived(log);
}

void DeviceBridge::DeviceEventCallback(const idevice_event_t *event, void *userdata)
{
    DeviceBridge::Get()->TriggerUpdateDevices();
}

void DeviceBridge::SystemLogsCallback(char c, void *user_data)
{
    LogPacket packet;
    if(ParseSystemLogs(c, packet))
    {
        DeviceBridge::Get()->TriggerSystemLogsReceived(packet);
    }
}
