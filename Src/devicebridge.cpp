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
            emit DeviceInfoReceived(m_deviceInfo);

            //start services
            StartInstaller();
            StartAFC();
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

void DeviceBridge::StartDiagnostics(DiagnosticsMode mode)
{
    diagnostics_relay_client_t diagnostics_client = NULL;
    lockdownd_service_descriptor_t service = NULL;

    /*  attempt to use newer diagnostics service available on iOS 5 and later */
    lockdownd_error_t ret = lockdownd_start_service(m_client, "com.apple.mobile.diagnostics_relay", &service);
    if (ret != LOCKDOWN_E_SUCCESS) {
        /*  attempt to use older diagnostics service */
        ret = lockdownd_start_service(m_client, "com.apple.iosdiagnostics.relay", &service);
    }

    if (ret != LOCKDOWN_E_SUCCESS) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to lockdownd: " + QString::number(ret), QMessageBox::Ok);
        return;
    }

    diagnostics_relay_error_t result = diagnostics_relay_client_new(m_device, service, &diagnostics_client);
    lockdownd_service_descriptor_free(service);

    if (result == DIAGNOSTICS_RELAY_E_SUCCESS) {
        switch (mode) {
        case CMD_SLEEP:
            if (diagnostics_relay_sleep(diagnostics_client) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Putting device into deep sleep mode.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to put device into deep sleep mode.", QMessageBox::Ok);
            }
            break;
        case CMD_RESTART:
            if (diagnostics_relay_restart(diagnostics_client, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Restarting device.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to restart device.", QMessageBox::Ok);
            }
            break;
        case CMD_SHUTDOWN:
            if (diagnostics_relay_shutdown(diagnostics_client, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS) {
                QMessageBox::information(m_mainWidget, "Info", "Shutting down device.", QMessageBox::Ok);
            } else {
                QMessageBox::critical(m_mainWidget, "Error", "ERROR: Failed to shutdown device.", QMessageBox::Ok);
            }
            break;
        default:
            break;
        }

        diagnostics_relay_goodbye(diagnostics_client);
        diagnostics_relay_client_free(diagnostics_client);
    } else {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to diagnostics_relay!", QMessageBox::Ok);
    }
}

void DeviceBridge::StartInstaller()
{
    if (!m_installer) {
        lockdownd_service_descriptor_t service = nullptr;
        lockdownd_error_t lerr = lockdownd_start_service(m_client, "com.apple.mobile.installation_proxy", &service);
        if (lerr != LOCKDOWN_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to lockdownd: " + QString::number(lerr), QMessageBox::Ok);
            return;
        }

        instproxy_error_t err = instproxy_client_new(m_device, service, &m_installer);
        lockdownd_service_descriptor_free(service);
        if (err != INSTPROXY_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to installation_proxy! " + QString::number(err), QMessageBox::Ok);
            return;
        }
    }
}

void DeviceBridge::StartAFC()
{
    if (!m_afc) {
        lockdownd_service_descriptor_t service = nullptr;
        lockdownd_error_t lerr = lockdownd_start_service(m_client, "com.apple.afc", &service);
        if (lerr != LOCKDOWN_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not start com.apple.afc! " + QString::number(lerr), QMessageBox::Ok);
            return;
        }

        afc_error_t err = afc_client_new(m_device, service, &m_afc);
        lockdownd_service_descriptor_free(service);
        if (err != AFC_E_SUCCESS) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not connect to AFC! " + QString::number(err), QMessageBox::Ok);
            return;
        }
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
