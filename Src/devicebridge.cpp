#include "devicebridge.h"
#include "extended_plist.h"
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

DeviceBridge::DeviceBridge()
    : m_device(nullptr)
    , m_client(nullptr)
    , m_diagnostics(nullptr)
    , m_screenshot(nullptr)
    , m_afc(nullptr)
    , m_imageMounter(nullptr)
    , m_crashlog(nullptr)
    , m_fileManager(nullptr)
    , m_houseArrest(nullptr)
    , m_installer(nullptr)
    , m_syslog(nullptr)
    , m_logHandler(new LogFilterThread())
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
    StopDebugging();

    if(m_client)
    {
        //Quick fix: stuck while reseting connection at exit, switch, reconnect
        //is_exist ? (void)lockdownd_client_free(m_client) : free(m_client);
        m_client = nullptr;
    }

    if (m_screenshot)
    {
        screenshotr_client_free(m_screenshot);
        m_screenshot = nullptr;
    }

    if (m_imageMounter)
    {
        if (is_exist)
            mobile_image_mounter_hangup(m_imageMounter);
        mobile_image_mounter_free(m_imageMounter);
        m_imageMounter = nullptr;
    }

    if (m_afc)
    {
        afc_client_free(m_afc);
        m_afc = nullptr;
    }

    if (m_crashlog)
    {
        afc_client_free(m_crashlog);
        m_crashlog = nullptr;
    }

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

    if (m_installer)
    {
        instproxy_client_free(m_installer);
        m_installer = nullptr;
    }

    if (m_syslog)
    {
        syslog_relay_client_free(m_syslog);
        m_syslog = nullptr;
    }

    if(m_device)
    {
        //Quick fix: crash when try to connect unpaired device
        //idevice_free(m_device);
        m_device = nullptr;
    }
    emit DeviceStatus(ConnectionStatus::DISCONNECTED, m_currentUdid, m_isRemote);
}

void DeviceBridge::ConnectToDevice(QString udid)
{
    AsyncManager::Get()->StartAsyncRequest([this, udid]() {
        emit ProcessStatusChanged(0, "Reset previous connection...");
        ResetConnection();

        //connect to udid
        emit ProcessStatusChanged(10, "Connecting to " + udid + "...");
        idevice_new_with_options(&m_device, udid.toStdString().c_str(), m_deviceList[udid] == CONNECTION_USBMUXD ? IDEVICE_LOOKUP_USBMUX : IDEVICE_LOOKUP_NETWORK);
        if (!m_device) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: No device with UDID " + udid);
            return;
        }

        emit ProcessStatusChanged(15, "Handshaking client...");
        if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(m_device, &m_client, TOOL_NAME)) {
            idevice_free(m_device);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Connecting to " + udid + " failed!");
            return;
        }

        emit ProcessStatusChanged(20, "Getting device info...");
        m_currentUdid = udid;
        m_isRemote = false;
        UpdateDeviceInfo();
        emit ProcessStatusChanged(100, "Connected to " + GetDeviceInfo()["DeviceName"].toString() + "!");
    });
}

void DeviceBridge::ConnectToDevice(QString ipAddress, int port)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        emit ProcessStatusChanged(0, "Reset previous connection...");
        ResetConnection();

        //connect to udid
        emit ProcessStatusChanged(10, QString::asprintf("Connecting to %s:%d...", ipAddress.toUtf8().data(), port));

        idevice_new_remote(&m_device, ipAddress.toStdString().c_str(), port);
        if (!m_device) {
            emit MessagesReceived(MessagesType::MSG_ERROR, QString::asprintf("ERROR: No device with %s:%d!", ipAddress.toUtf8().data(), port));
            return;
        }

        emit ProcessStatusChanged(15, "Handshaking client...");
        lockdownd_error_t ret = lockdownd_client_new_with_handshake_remote(m_device, &m_client, TOOL_NAME);
        if (LOCKDOWN_E_SUCCESS != ret) {
            idevice_free(m_device);
            emit MessagesReceived(MessagesType::MSG_ERROR, QString::asprintf("ERROR: Connecting to %s:%d failed!", ipAddress.toUtf8().data(), port));
            return;
        }

        emit ProcessStatusChanged(20, "Getting device info...");
        m_isRemote = true;
        UpdateDeviceInfo();
        emit ProcessStatusChanged(100, "Connected to " + GetDeviceInfo()["DeviceName"].toString() + "!");
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
    if(lockdownd_get_value(m_client, nullptr, nullptr, &node) == LOCKDOWN_E_SUCCESS) {
        if (node) {
            QJsonDocument deviceInfo = PlistToJson(node);
            m_currentUdid = deviceInfo["UniqueDeviceID"].toString();
            m_deviceInfo[m_currentUdid] = deviceInfo;
            plist_free(node);
            node = nullptr;
            emit DeviceStatus(ConnectionStatus::CONNECTED, m_currentUdid, m_isRemote);

            //start services
            StartServices();
        }
    }
}

QJsonDocument DeviceBridge::GetDeviceInfo(QString udid)
{
    return m_deviceInfo[udid.isEmpty() ? m_currentUdid : udid];
}

void DeviceBridge::StartServices()
{
    emit ProcessStatusChanged(30, "Starting installation proxy service...");
    QStringList serviceIds = QStringList() << "com.apple.mobile.installation_proxy";
    StartLockdown(!m_installer, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        instproxy_error_t err = instproxy_client_new(m_device, service, &m_installer);
        if (err != INSTPROXY_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
    });

    emit ProcessStatusChanged(35, "Getting installed apps info...");
    m_installedApps = GetInstalledApps(false);

    emit ProcessStatusChanged(40, "Starting crash report mover service...");
    serviceIds = QStringList() << "com.apple.crashreportmover";
    StartLockdown(!m_crashlog, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        service_client_t svcmove = NULL;
        service_error_t err = service_client_new(m_device, service, &svcmove);
        if (err != SERVICE_E_SUCCESS)
        {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
            return;
        }

        /* read "ping" message which indicates the crash logs have been moved to a safe harbor */
        char* ping = (char*)malloc(4);
        memset(ping, '\0', 4);
        int attempts = 0;
        while ((strncmp(ping, "ping", 4) != 0) && (attempts < 10)) {
            uint32_t bytes = 0;
            err = service_receive_with_timeout(svcmove, ping, 4, &bytes, 2000);
            if (err == SERVICE_E_SUCCESS || err == SERVICE_E_TIMEOUT) {
                attempts++;
                continue;
            }

            fprintf(stderr, "ERROR: Crash logs could not be moved. Connection interrupted (%d).\n", err);
            break;
        }
        service_client_free(svcmove);
        free(ping);

        if (attempts >= 10) {
            fprintf(stderr, "ERROR: Failed to receive ping message from crash report mover.\n");
        }
    });

    emit ProcessStatusChanged(50, "Starting crash report copy service...");
    serviceIds = QStringList() << "com.apple.crashreportcopymobile";
    StartLockdown(!m_crashlog, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        afc_error_t err = afc_client_new(m_device, service, &m_crashlog);
        if (err != AFC_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
    });

    emit ProcessStatusChanged(60, "Starting afc service...");
    serviceIds = QStringList() << "com.apple.afc";
    StartLockdown(!m_afc, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        afc_error_t err = afc_client_new(m_device, service, &m_afc);
        if (err != AFC_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
    });

    emit ProcessStatusChanged(70, "Starting image mounter service...");
    serviceIds = QStringList() << MOBILE_IMAGE_MOUNTER_SERVICE_NAME;
    StartLockdown(!m_imageMounter, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        mobile_image_mounter_error_t err = mobile_image_mounter_new(m_device, service, &m_imageMounter);
        if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
    });

    emit ProcessStatusChanged(80, "Starting syslog relay service...");
    serviceIds = QStringList() << SYSLOG_RELAY_SERVICE_NAME;
    StartLockdown(!m_syslog, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        /* connect to syslog_relay service */
        syslog_relay_error_t err = SYSLOG_RELAY_E_UNKNOWN_ERROR;
        err = syslog_relay_client_new(m_device, service, &m_syslog);
        if (err != SYSLOG_RELAY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
            return;
        }

        /* start capturing syslog */
        err = syslog_relay_start_capture_raw(m_syslog, SystemLogsCallback, nullptr);
        if (err != SYSLOG_RELAY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Unable to start capturing syslog.");
            syslog_relay_client_free(m_syslog);
            m_syslog = nullptr;
            return;
        }
    });
}

void DeviceBridge::StartLockdown(bool condition, QStringList service_ids, const std::function<void (QString&, lockdownd_service_descriptor_t&)> &function)
{
    if (!condition)
        return;

    lockdownd_error_t lerr = lockdownd_error_t::LOCKDOWN_E_UNKNOWN_ERROR;
    lockdownd_service_descriptor_t service = nullptr;
    QString service_id;
    for ( const auto& svc_id : service_ids)
    {
        service_id = svc_id;
        lerr = lockdownd_start_service(m_client, svc_id.toUtf8().data(), &service);
        if(lerr == LOCKDOWN_E_SUCCESS) { break; }
    }

    switch (lerr)
    {
        case LOCKDOWN_E_SUCCESS:
            function(service_id, service);
            lockdownd_service_descriptor_free(service);
            break;

        case LOCKDOWN_E_PASSWORD_PROTECTED:
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Device is passcode protected, enter passcode on the device to continue.");
            break;

        default:
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " lockdownd: " + QString::number(lerr));
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
