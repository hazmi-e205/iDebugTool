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

    // create mew client
    m_clients[op] = m_isRemote ? new DeviceClient(m_remoteAddress, service_ids, service_ids_2) : new DeviceClient(m_currentUdid, service_ids, service_ids_2);
    bool ok = m_clients[op]->device_error == IDEVICE_E_SUCCESS && m_clients[op]->lockdownd_error == LOCKDOWN_E_SUCCESS;
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
    if (m_clients.contains(operation))
    {
        delete m_clients[operation];
        m_clients.remove(operation);
    }
}

lockdownd_service_descriptor_t DeviceBridge::GetService(MobileOperation operation, QStringList service_ids)
{
    for (auto& id : service_ids)
    {
        if (m_clients[operation]->services.contains(id))
            return m_clients[operation]->services[id];
    }
    RemoveClient(operation);
    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: No available service!" + service_ids.join(", "));
    return nullptr;
}

DeviceBridge::DeviceBridge()
    : m_device(nullptr)
    , m_fileClient(nullptr)
    , m_fileManager(nullptr)
    , m_houseArrest(nullptr)
    , m_logHandler(new LogFilterThread())
    , m_debugClient(nullptr)
    , m_debugger(nullptr)
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
    bool is_exist = m_deviceList.find(m_currentUdid) != m_deviceList.end();
    m_currentUdid.clear();
    m_remoteAddress.clear();
    StopDebugging();
    StopSyslog();

    for (auto& client : m_clients.keys())
    {
        delete m_clients[client];
    }
    m_clients.clear();

    if (m_fileManager)
    {
        afc_client_free(m_fileManager);
        m_fileManager = nullptr;
    }

    if (m_houseArrest)
    {
        house_arrest_client_free(m_houseArrest);
        m_houseArrest = nullptr;
    }
    
    //Quick fix: stuck while reseting connection at exit, switch, reconnect just comment free
    if(m_fileClient)
    {
        lockdownd_client_free(m_fileClient);
        m_fileClient = nullptr;
    }

    if(m_debugClient)
    {
        lockdownd_client_free(m_debugClient);
        m_debugClient = nullptr;
    }

    if(m_device)
    {
        //Quick fix: crash when try to connect unpaired device
        idevice_free(m_device);
        m_device = nullptr;
    }
    emit DeviceStatus(ConnectionStatus::DISCONNECTED, m_currentUdid, m_isRemote);
}

void DeviceBridge::ConnectToDevice(const std::function<void()>& configureConnection)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        emit ProcessStatusChanged(0, "Reset previous connection...");
        ResetConnection();

        emit ProcessStatusChanged(10, m_isRemote ? QString("Connecting to %1:%2...").arg(m_remoteAddress.ipAddress, m_remoteAddress.port) : QString("Connecting to %1...").arg(m_currentUdid));
        configureConnection();
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
    plist_t node = nullptr;
    if(lockdownd_get_value(m_clients[MobileOperation::DEVICE_INFO]->client, nullptr, nullptr, &node) == LOCKDOWN_E_SUCCESS) {
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

void DeviceBridge::StartLockdown(bool condition, lockdownd_client_t& client, QStringList service_ids, const std::function<void (QString&, lockdownd_service_descriptor_t&)> &function, bool clear_lockdownd)
{
    if (!condition)
        return;

    lockdownd_error_t err = lockdownd_error_t::LOCKDOWN_E_UNKNOWN_ERROR;
    if (client == nullptr)
    {
        err = m_isRemote ? lockdownd_client_new_with_handshake_remote(m_device, &client, TOOL_NAME) : lockdownd_client_new_with_handshake(m_device, &client, TOOL_NAME);
        if (LOCKDOWN_E_SUCCESS != err)
        {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Create lockdownd handshake failed!");
            client = nullptr;
            return;
        }
    }

    if (service_ids.size() > 0)
        err = lockdownd_error_t::LOCKDOWN_E_UNKNOWN_ERROR;

    lockdownd_service_descriptor_t service = nullptr;
    QString service_id;
    for ( const auto& svc_id : service_ids)
    {
        service_id = svc_id;
        err = lockdownd_start_service(client, svc_id.toUtf8().data(), &service);
        if(err == LOCKDOWN_E_SUCCESS) { break; }
    }

    switch (err)
    {
        case LOCKDOWN_E_SUCCESS:
            function(service_id, service);
            lockdownd_service_descriptor_free(service);
            if (clear_lockdownd && !client) {
                lockdownd_client_free(client);
                client = nullptr;
            }
            break;

        case LOCKDOWN_E_PASSWORD_PROTECTED:
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Device is passcode protected, enter passcode on the device to continue.");
            break;

        case LOCKDOWN_E_SSL_ERROR:
            if (m_isRemote)
                ConnectToDevice(m_remoteAddress.ipAddress, m_remoteAddress.port);
            else
                ConnectToDevice(m_currentUdid);
            break;

        default:
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " lockdownd: " + QString::number(err));
            break;
    }
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
