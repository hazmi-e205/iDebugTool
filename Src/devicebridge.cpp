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

void DeviceBridge::TriggerUpdateDevices()
{
    emit UpdateDevices(GetDevices());
}

void DeviceBridge::DeviceEventCallback(const idevice_event_t *event, void *userdata)
{
    DeviceBridge::Get()->TriggerUpdateDevices();
}
