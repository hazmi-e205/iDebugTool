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
    m_installer(nullptr),
    m_afc(nullptr),
    m_diagnostics(nullptr),
    m_imageMounter(nullptr),
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

std::map<QString, idevice_connection_type> DeviceBridge::GetDevices()
{
    std::map<QString, idevice_connection_type> pDevices;
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
            pDevices[dev_list[i]->udid] = dev_list[i]->conn_type;
        }
        idevice_device_list_extended_free(dev_list);
    }
    return pDevices;
}

void DeviceBridge::ResetConnection()
{
    if (m_imageMounter)
    {
        mobile_image_mounter_hangup(m_imageMounter);
        mobile_image_mounter_free(m_imageMounter);
        m_imageMounter = nullptr;
    }

    if (m_diagnostics)
    {
        diagnostics_relay_goodbye(m_diagnostics);
        diagnostics_relay_client_free(m_diagnostics);
        m_diagnostics = nullptr;
    }

    if (m_afc)
    {
        afc_client_free(m_afc);
        m_afc = nullptr;
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

void DeviceBridge::ConnectToDevice(QString udid, idevice_connection_type type)
{
    ResetConnection();

    //connect to udid
    idevice_new_with_options(&m_device, udid.toStdString().c_str(), type == CONNECTION_USBMUXD ? IDEVICE_LOOKUP_USBMUX : IDEVICE_LOOKUP_NETWORK);
    if (!m_device) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: No device with UDID " + udid, QMessageBox::Ok);
        return;
    }
    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(m_device, &m_client, TOOL_NAME)) {
        idevice_free(m_device);
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Connecting to " + udid + " failed!", QMessageBox::Ok);
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
            emit DeviceInfoReceived(m_deviceInfo);

            //start services
            StartServices();
        }
    }
}

void DeviceBridge::StartServices()
{
    QStringList serviceIds;
    serviceIds = QStringList() << SYSLOG_RELAY_SERVICE_NAME;
    StartLockdown(!m_installer, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        /* connect to syslog_relay service */
        syslog_relay_error_t err = SYSLOG_RELAY_E_UNKNOWN_ERROR;
        err = syslog_relay_client_new(m_device, service, &m_syslog);
        if (err != SYSLOG_RELAY_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " client! " + QString::number(err), QMessageBox::Ok);
            return;
        }

        /* start capturing syslog */
        err = syslog_relay_start_capture_raw(m_syslog, SystemLogsCallback, nullptr);
        if (err != SYSLOG_RELAY_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Unable to start capturing syslog.", QMessageBox::Ok);
            syslog_relay_client_free(m_syslog);
            m_syslog = nullptr;
            return;
        }
    });

    serviceIds = QStringList() << "com.apple.mobile.installation_proxy";
    StartLockdown(!m_installer, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        instproxy_error_t err = instproxy_client_new(m_device, service, &m_installer);
        if (err != INSTPROXY_E_SUCCESS)
        {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " client! " + QString::number(err), QMessageBox::Ok);
        }
    });

    serviceIds = QStringList() << "com.apple.afc";
    StartLockdown(!m_afc, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        afc_error_t err = afc_client_new(m_device, service, &m_afc);
        if (err != AFC_E_SUCCESS)
        {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " client! " + QString::number(err), QMessageBox::Ok);
        }
    });

    serviceIds = QStringList() << "com.apple.mobile.mobile_image_mounter";
    StartLockdown(!m_imageMounter, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        mobile_image_mounter_error_t err = mobile_image_mounter_new(m_device, service, &m_imageMounter);
        if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS)
        {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " client! " + QString::number(err), QMessageBox::Ok);
        }
    });

    serviceIds = QStringList() << "com.apple.mobile.diagnostics_relay" << "com.apple.iosdiagnostics.relay";
    StartLockdown(!m_diagnostics, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
        diagnostics_relay_error_t err = diagnostics_relay_client_new(m_device, service, &m_diagnostics);
        if (err != DIAGNOSTICS_RELAY_E_SUCCESS)
        {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " client! " + QString::number(err), QMessageBox::Ok);
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
    for ( const auto& service_id : service_ids)
    {
        lerr = lockdownd_start_service(m_client, service_id.toUtf8().data(), &service);
        if(lerr == LOCKDOWN_E_SUCCESS) { break; }
    }

    switch (lerr)
    {
        case LOCKDOWN_E_SUCCESS:
            function(service_id, service);
            lockdownd_service_descriptor_free(service);
            break;

        case LOCKDOWN_E_PASSWORD_PROTECTED:
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Device is passcode protected, enter passcode on the device to continue.", QMessageBox::Ok);
            break;

        default:
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to " + service_id + " lockdownd: " + QString::number(lerr), QMessageBox::Ok);
            break;
    }
}

void DeviceBridge::StartDiagnostics(DiagnosticsMode mode)
{
    if (m_diagnostics) {
        switch (mode) {
        case CMD_SLEEP:
            if (diagnostics_relay_sleep(m_diagnostics) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Putting device into deep sleep mode.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to put device into deep sleep mode.", QMessageBox::Ok);
            }
            break;
        case CMD_RESTART:
            if (diagnostics_relay_restart(m_diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Restarting device.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to restart device.", QMessageBox::Ok);
            }
            break;
        case CMD_SHUTDOWN:
            if (diagnostics_relay_shutdown(m_diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Shutting down device.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to shutdown device.", QMessageBox::Ok);
            }
            break;
        default:
            break;
        }

    } else {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to diagnostics_relay!", QMessageBox::Ok);
    }
}

QStringList DeviceBridge::GetMountedImages()
{
    QStringList signatures;
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
        QMessageBox::critical(m_mainWidget, "Error", "Error: lookup_image returned " + QString::number(err), QMessageBox::Ok);
    }

    if (result)
        plist_free(result);
    return signatures;
}

void DeviceBridge::MountImage(QString image_path, QString signature_path)
{
    char sig[8192];
    size_t sig_length = 0;
    size_t image_size = 0;
    mobile_image_mounter_error_t err = MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
    plist_t result = NULL;

    FILE *f = fopen(signature_path.toUtf8().data(), "rb");
    if (!f) {
        QMessageBox::critical(m_mainWidget, "Error", "Error: opening signature file '" + signature_path + "' : " + strerror(errno), QMessageBox::Ok);
        return;
    }
    sig_length = fread(sig, 1, sizeof(sig), f);
    fclose(f);
    if (sig_length == 0) {
        QMessageBox::critical(m_mainWidget, "Error", "Error: Could not read signature from file '" + signature_path + "'", QMessageBox::Ok);
        return;
    }

    f = fopen(image_path.toUtf8().data(), "rb");
    if (!f) {
        QMessageBox::critical(m_mainWidget, "Error", "Error: opening image file '" + image_path + "' : " + strerror(errno), QMessageBox::Ok);
        return;
    }

    struct stat fst;
    if (stat(image_path.toUtf8().data(), &fst) != 0) {
        QMessageBox::critical(m_mainWidget, "Error", "Error: stat: '" + image_path + "' : " + strerror(errno), QMessageBox::Ok);
        return;
    }
    image_size = fst.st_size;
    if (stat(signature_path.toUtf8().data(), &fst) != 0) {
        QMessageBox::critical(m_mainWidget, "Error", "Error: stat: '" + signature_path + "' : " + strerror(errno), QMessageBox::Ok);
        return;
    }

    QString targetname = QString(PKG_PATH) + "/staging.dimage";
    QString mountname = QString(PATH_PREFIX) + "/" + targetname;

    MounterType mount_type = DISK_IMAGE_UPLOAD_TYPE_AFC;
    QStringList os_version = m_deviceInfo["ProductVersion"].toString().split(".");
    if (os_version[0].toInt() >= 7) {
        mount_type = DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE;
    }

    switch (mount_type) {
        case DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE:
            qDebug() << "Uploading " + image_path;
            err = mobile_image_mounter_upload_image(m_imageMounter, "Developer", image_size, sig, sig_length, ImageMounterCallback, f);
            if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
                QString message("ERROR: Unknown error occurred, can't mount.");
                if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
                    message = "ERROR: Device is locked, can't mount. Unlock device and try again.";
                }
                QMessageBox::critical(m_mainWidget, "Error", "Error: " + message, QMessageBox::Ok);
                return;
            }
            break;

        default:
            qDebug() << "Uploading " + image_path + " --> afc:///" + targetname;
            char **strs = NULL;
            if (afc_get_file_info(m_afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
                if (afc_make_directory(m_afc, PKG_PATH) != AFC_E_SUCCESS) {
                    fprintf(stderr, "WARNING: Could not create directory '%s' on device!\n", PKG_PATH);
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
                QMessageBox::critical(m_mainWidget, "Error", "Error: afc_file_open on '" + targetname + "' failed!", QMessageBox::Ok);
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
                            QMessageBox::critical(m_mainWidget, "Error", "Error: AFC Write error!", QMessageBox::Ok);
                            break;
                        }
                        total += written;
                    }
                    if (total != amount) {
                        QMessageBox::critical(m_mainWidget, "Error", "Error: wrote only " + QString::number(total) + " of " + QString::number(amount), QMessageBox::Ok);
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
    qDebug() << "done.";

    qDebug() << "Mounting...";
    err = mobile_image_mounter_mount_image(m_imageMounter, mountname.toUtf8().data(), sig, sig_length, "Developer", &result);
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
    {
        QMessageBox::information(m_mainWidget, "Mount Success!", "Developer disk image mounted", QMessageBox::Ok);
    }
    else
    {
        QMessageBox::critical(m_mainWidget, "Error", "Error: mount_image returned " + QString::number(err), QMessageBox::Ok);
    }
    if (result)
        plist_free(result);
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

ssize_t DeviceBridge::ImageMounterCallback(void *buf, size_t size, void *userdata)
{
    return fread(buf, 1, size, (FILE*)userdata);
}
