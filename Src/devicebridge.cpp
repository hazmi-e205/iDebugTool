#include "devicebridge.h"
#include "extended_plist.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>

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
    , m_imageMounter(nullptr)
    , m_screenshot(nullptr)
    , m_afc(nullptr)
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
        lockdownd_error_t ret = lockdownd_client_new_with_handshake(m_device, &m_client, TOOL_NAME);
        if (LOCKDOWN_E_SUCCESS != ret) {
            idevice_free(m_device);
            emit MessagesReceived(MessagesType::MSG_ERROR, QString::asprintf("ERROR: Connecting to %s:%d failed!", ipAddress.toUtf8().data(), port));
            return;
        }

        emit ProcessStatusChanged(20, "Getting device info...");
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
            emit DeviceConnected();

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

void DeviceBridge::StartDiagnostics(DiagnosticsMode mode)
{
    QStringList serviceIds = QStringList() << "com.apple.mobile.diagnostics_relay" << "com.apple.iosdiagnostics.relay";
    StartLockdown(!m_diagnostics, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        diagnostics_relay_error_t err = diagnostics_relay_client_new(m_device, service, &m_diagnostics);
        if (err != DIAGNOSTICS_RELAY_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
    });

    if (m_diagnostics)
    {
        switch (mode)
        {
        case CMD_SLEEP:
            if (diagnostics_relay_sleep(m_diagnostics) == DIAGNOSTICS_RELAY_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_INFO, "Putting device into deep sleep mode.");
            else
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to put device into deep sleep mode.");
            break;
        case CMD_RESTART:
            if (diagnostics_relay_restart(m_diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_INFO, "Restarting device.");
            else
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to restart device.");
            break;
        case CMD_SHUTDOWN:
            if (diagnostics_relay_shutdown(m_diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_INFO, "Shutting down device.");
            else
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to shutdown device.");
            break;
        default:
            break;
        }

        diagnostics_relay_goodbye(m_diagnostics);
        diagnostics_relay_client_free(m_diagnostics);
        m_diagnostics = nullptr;
    }
    else
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to diagnostics_relay!");
    }
}

QStringList DeviceBridge::GetMountedImages()
{
    QStringList signatures;
    if (!m_imageMounter)
        return signatures;

    QJsonDocument doc;
    plist_t result = nullptr;
    mobile_image_mounter_error_t err = mobile_image_mounter_lookup_image(m_imageMounter, "Developer", &result);
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
    {
        doc = PlistToJson(result);
        auto arr = doc["ImageSignature"].toArray();
        for (int idx = 0; idx < arr.count(); idx++)
        {
            signatures.append(arr[idx].toString());
        }
    }
    else
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, "Error: lookup_image returned " + QString::number(err));
    }

    if (result)
        plist_free(result);
    return signatures;
}

bool DeviceBridge::IsImageMounted()
{
    QStringList mounted = DeviceBridge::Get()->GetMountedImages();
    return !mounted.empty();
}

void DeviceBridge::MountImage(QString image_path, QString signature_path)
{
    AsyncManager::Get()->StartAsyncRequest([this, image_path, signature_path]() {
        unsigned char *sig = NULL;
        size_t sig_length = 0;
        size_t image_size = 0;
        mobile_image_mounter_error_t err = MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
        plist_t result = NULL;

        FILE *f = fopen(signature_path.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening signature file '" + signature_path + "' : " + strerror(errno));
            return;
        }
        sig_length = fread(sig, 1, sizeof(sig), f);
        fclose(f);
        if (sig_length == 0) {
            emit MounterStatusChanged("Error: Could not read signature from file '" + signature_path + "'");
            return;
        }

        f = fopen(image_path.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening image file '" + image_path + "' : " + strerror(errno));
            return;
        }

        // struct stat fst;
        // if (stat(image_path.toUtf8().data(), &fst) != 0) {
        //     emit MounterStatusChanged("Error: stat: '" + image_path + "' : " + strerror(errno));
        //     return;
        // }
        // image_size = fst.st_size;
        // if (stat(signature_path.toUtf8().data(), &fst) != 0) {
        //     emit MounterStatusChanged("Error: stat: '" + signature_path + "' : " + strerror(errno));
        //     return;
        // }

        QString targetname = QString(PKG_PATH) + "/staging.dimage";
        QString mountname = QString(PATH_PREFIX) + "/" + targetname;

        MounterType mount_type = DISK_IMAGE_UPLOAD_TYPE_AFC;
        QStringList os_version = m_deviceInfo[m_currentUdid]["ProductVersion"].toString().split(".");
        if (os_version[0].toInt() >= 7) {
            mount_type = DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE;
        }

        switch (mount_type) {
        case DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE:
            emit MounterStatusChanged("Uploading '" + QFileInfo(image_path).fileName() + "' to device...");
            err = mobile_image_mounter_upload_image(m_imageMounter, "Developer", image_size, sig, sig_length, ImageMounterCallback, f);
            if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
                QString message("ERROR: Unknown error occurred, can't mount.");
                if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
                    message = "ERROR: Device is locked, can't mount. Unlock device and try again.";
                }
                emit MounterStatusChanged("Error: " + message);
                return;
            }
            break;

        default:
            emit MounterStatusChanged("Uploading " + QFileInfo(image_path).fileName() + " --> afc:///" + targetname);
            char **strs = NULL;
            if (afc_get_file_info(m_afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
                if (afc_make_directory(m_afc, PKG_PATH) != AFC_E_SUCCESS) {
                    emit MounterStatusChanged("WARNING: Could not create directory '" + QString(PKG_PATH) + "' on device!\n");
                }
            }
            if (strs) {
                int i = 0;
                while (strs[i]) {
                    free(strs[i]);
                    i++;
                }
                free(strs);
            }

            uint64_t af = 0;
            if ((afc_file_open(m_afc, targetname.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
                fclose(f);
                emit MounterStatusChanged("Error: afc_file_open on '" + targetname + "' failed!");
                return;
            }

            char buf[8192];
            size_t amount = 0;
            do {
                amount = fread(buf, 1, sizeof(buf), f);
                if (amount > 0) {
                    uint32_t written, total = 0;
                    while (total < amount) {
                        written = 0;
                        if (afc_file_write(m_afc, af, buf + total, amount - total, &written) != AFC_E_SUCCESS) {
                            emit MounterStatusChanged("Error: AFC Write error!");
                            break;
                        }
                        total += written;
                    }
                    if (total != amount) {
                        emit MounterStatusChanged("Error: wrote only " + QString::number(total) + " of " + QString::number(amount));
                        afc_file_close(m_afc, af);
                        fclose(f);
                        return;
                    }
                }
            }
            while (amount > 0);

            afc_file_close(m_afc, af);
            break;
        }
        fclose(f);
        emit MounterStatusChanged("Image uploaded.");

        emit MounterStatusChanged("Mounting...");
        err = mobile_image_mounter_mount_image(m_imageMounter, mountname.toUtf8().data(), sig, sig_length, "Developer", &result);
        if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
        {
            emit MounterStatusChanged("Developer disk image mounted");
            ConnectToDevice(m_currentUdid); //hack to fix LOCKDOWN_E_MUX_ERROR after mounted
        }
        else
        {
            emit MounterStatusChanged("Error: mount_image returned " + QString::number(err));
        }

        if (result)
        {
            emit MounterStatusChanged(PlistToJson(result).toJson());
            plist_free(result);
        }
    });
}

void DeviceBridge::Screenshot(QString path)
{
    AsyncManager::Get()->StartAsyncRequest([this, path]() {
        QStringList serviceIds = QStringList() << SCREENSHOTR_SERVICE_NAME;
        StartLockdown(!m_screenshot, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
            screenshotr_error_t err = screenshotr_client_new(m_device, service, &m_screenshot);
            if (err != SCREENSHOTR_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
        });

        char *imgdata = NULL;
        uint64_t imgsize = 0;
        screenshotr_error_t error = screenshotr_take_screenshot(m_screenshot, &imgdata, &imgsize);
        if (error == SCREENSHOTR_E_SUCCESS)
        {
            QFileInfo file_info(path);
            QDir().mkpath(file_info.filePath().remove(file_info.fileName()));
            QSaveFile file(path);
            file.open(QIODevice::WriteOnly);
            file.write(imgdata, imgsize);
            file.commit();
            emit ScreenshotReceived(path);
        }
        else
        {
            emit MessagesReceived(MessagesType::MSG_ERROR, "Error: screenshotr_take_screenshot returned " + QString::number(error));
        }
    });
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

ssize_t DeviceBridge::ImageMounterCallback(void *buf, size_t size, void *userdata)
{
    if (!m_destroyed)
        return fread(buf, 1, size, (FILE*)userdata);
}
