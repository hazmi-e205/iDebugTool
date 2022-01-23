#include "devicebridge.h"
#include "utils.h"
#include <QDebug>

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

DeviceBridge::DeviceBridge(QObject *parent) :
    QObject(parent),
    m_device(nullptr),
    m_client(nullptr)
{
}

DeviceBridge::~DeviceBridge()
{
    idevice_event_unsubscribe();
    ResetConnection();
}

void DeviceBridge::Init()
{
    idevice_event_subscribe(DeviceEventCallback, nullptr);
}

std::vector<Device> DeviceBridge::GetDevices()
{
    std::vector<Device> pDevices;
    idevice_info_t *dev_list = NULL;
    int i;
    if (idevice_get_device_list_extended(&dev_list, &i) < 0)
    {
        qDebug() << "ERROR: Unable to retrieve device list!";
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
        qDebug() << "ERROR: No device with UDID " + device.udid + " attached.";
        return;
    }
    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(m_device, &m_client, TOOL_NAME)) {
        idevice_free(m_device);
        qDebug() << "ERROR: Connecting to device failed!";
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
    std::vector<Device> pDevices = GetDevices();
    if (pDevices.size() == 0)
        ResetConnection();

    emit UpdateDevices(pDevices);
}

void DeviceBridge::DeviceEventCallback(const idevice_event_t *event, void *userdata)
{
    DeviceBridge::Get()->TriggerUpdateDevices();
}
