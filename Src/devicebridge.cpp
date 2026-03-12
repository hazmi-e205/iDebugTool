#include "devicebridge.h"
#include "extended_plist.h"
#include "qforeach.h"
#include <QDebug>
#include <QMessageBox>

bool DeviceBridge::m_destroyed = false;
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
    m_destroyed = true;
}

bool DeviceBridge::CreateClient(MobileOperation op, QStringList service_ids, QStringList service_ids_2)
{
    // make sure it killed
    RemoveClient(op);

    m_cancelFlags[op] = std::make_shared<std::atomic_bool>(false);

    // create mew client
    std::shared_ptr<DeviceClient> client;
    if (m_isRemote)
        client = std::make_shared<DeviceClient>(m_remoteAddress, service_ids, service_ids_2);
    else
        client = std::make_shared<DeviceClient>(m_currentUdid, service_ids, service_ids_2);
    m_clients[op] = client;
    bool ok = client->device_error == IDEVICE_E_SUCCESS && client->lockdownd_error == LOCKDOWN_E_SUCCESS;
    if (!ok)
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, m_isRemote ? ("ERROR: No device with " + m_remoteAddress.toString())
                                                                  : ("ERROR: No device with UDID " + m_currentUdid));
        RemoveClient(op);
    }
    return ok;
}

void DeviceBridge::RemoveClient(MobileOperation operation)
{
    auto cancel = m_cancelFlags.value(operation);
    if (cancel) {
        cancel->store(true);
    }
    m_cancelFlags.remove(operation);

    if (m_clients.contains(operation))
    {
        m_clients.remove(operation);
    }
}

lockdownd_service_descriptor_t DeviceBridge::GetService(MobileOperation operation, QStringList service_ids)
{
    auto client = m_clients.value(operation);
    if (!client)
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: No active client for requested operation.");
        return nullptr;
    }

    for (auto& id : service_ids)
    {
        if (client->services.contains(id))
            return client->services[id];
    }
    RemoveClient(operation);
    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: No available service!" + service_ids.join(", "));
    return nullptr;
}

DeviceBridge::DeviceBridge()
    : m_logHandler(new LogFilterThread())
    , m_debugHandler(new DebuggerFilterThread())
{
    connect(m_logHandler, SIGNAL(FilterComplete(QString)), this, SIGNAL(SystemLogsReceived2(QString)));
    connect(m_logHandler, SIGNAL(FilterStatusChanged(bool)), this, SIGNAL(FilterStatusChanged(bool)));
    connect(m_debugHandler, SIGNAL(FilterComplete(QString)), this, SIGNAL(DebuggerReceived(QString)));
    connect(m_debugHandler, SIGNAL(FilterStatusChanged(bool)), this, SIGNAL(DebuggerFilterStatus(bool)));
}

DeviceBridge::~DeviceBridge()
{
    idevice_event_unsubscribe();
    ResetConnection();
    delete m_logHandler;
}

void DeviceBridge::Init(QWidget *parent)
{
    idevice_set_debug_level(1);
    idevice_event_subscribe(DeviceEventCallback, nullptr);
}

QMap<QString, idevice_connection_type> DeviceBridge::GetDevices()
{
    idevice_info_t *dev_list = NULL;
    int dev_count = 0;
    idevice_get_device_list_extended(&dev_list, &dev_count);

    m_deviceList.clear();
    for (int idx = 0; idx < dev_count; idx++)
    {
        m_deviceList[dev_list[idx]->udid] = dev_list[idx]->conn_type;
    }
    if (dev_list)
        idevice_device_list_extended_free(dev_list);
    return m_deviceList;
}

void DeviceBridge::ResetConnection()
{
    m_currentUdid.clear();
    m_remoteAddress.clear();
    StopDebugging();
    StopSyslog();
    StopOSTrace();

    for (auto& client : m_clients.keys())
    {
        auto cancel = m_cancelFlags.value(client);
        if (cancel) {
            cancel->store(true);
        }
    }
    m_clients.clear();
    m_cancelFlags.clear();
    emit DeviceStatus(ConnectionStatus::DISCONNECTED, m_currentUdid, m_isRemote);
}

void DeviceBridge::ConnectToDevice(const std::function<void()>& configureConnection)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        emit ProcessStatusChanged(0, "Reset previous connection...");
        ResetConnection();

        configureConnection();
        emit ProcessStatusChanged(10, m_isRemote ? QString("Connecting to %1 port %2 ...").arg(m_remoteAddress.ipAddress).arg(m_remoteAddress.port) : QString("Connecting to %1 ...").arg(m_currentUdid));
        if (!CreateClient(MobileOperation::DEVICE_INFO))
            return;

        emit ProcessStatusChanged(20, "Getting device info...");
        UpdateDeviceInfo();
        emit ProcessStatusChanged(100, "Connected to " + GetDeviceInfo()["DeviceName"].toString() + "!");
        emit DeviceStatus(ConnectionStatus::CONNECTED, m_currentUdid, m_isRemote);
    });
}

void DeviceBridge::ConnectToDevice(QString udid)
{
    ConnectToDevice(
        [this, udid]() {
            m_currentUdid = udid;
            m_isRemote = false;
        });
}

void DeviceBridge::ConnectToDevice(QString ipAddress, int port)
{
    ConnectToDevice(
        [this, ipAddress, port]() {
            m_remoteAddress = RemoteAddress(ipAddress, port);
            m_isRemote = true;
        });
}

QString DeviceBridge::GetCurrentUdid()
{
    return m_currentUdid;
}

bool DeviceBridge::IsConnected()
{
    return !m_currentUdid.isEmpty();
}

void DeviceBridge::UpdateDeviceInfo()
{
    auto client = m_clients.value(MobileOperation::DEVICE_INFO);
    if (!client)
        return;

    plist_t node = nullptr;
    if(lockdownd_get_value(client->client, nullptr, nullptr, &node) == LOCKDOWN_E_SUCCESS) {
        if (node) {
            QJsonDocument deviceInfo = PlistToJson(node);
            m_currentUdid = deviceInfo["UniqueDeviceID"].toString();
            m_deviceInfo[m_currentUdid] = deviceInfo;
            plist_free(node);
            node = nullptr;
        }
    }
    RemoveClient(MobileOperation::DEVICE_INFO);
}

QJsonDocument DeviceBridge::GetDeviceInfo(QString udid)
{
    return m_deviceInfo[udid.isEmpty() ? m_currentUdid : udid];
}

void DeviceBridge::TriggerUpdateDevices(idevice_event_type eventType, idevice_connection_type connectionType, QString udid)
{
    switch (eventType)
    {
    case idevice_event_type::IDEVICE_DEVICE_ADD:
        m_deviceList[udid] = connectionType;
        break;

    case idevice_event_type::IDEVICE_DEVICE_REMOVE:
        m_deviceList.remove(udid);
        if (m_currentUdid == udid)
            ResetConnection();
        break;

    default:
        break;
    }

    emit UpdateDevices(m_deviceList);
}

void DeviceBridge::DeviceEventCallback(const idevice_event_t *event, void *userdata)
{
    if (!m_destroyed)
        DeviceBridge::Get()->TriggerUpdateDevices(event->event, event->conn_type, event->udid);
}
