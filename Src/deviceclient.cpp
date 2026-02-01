#include "deviceclient.h"
#include <QDebug>

DeviceClient::DeviceClient()
    : device(nullptr)
    , client(nullptr)
    , lockdownd_error(LOCKDOWN_E_UNKNOWN_ERROR)
    , device_error(IDEVICE_E_UNKNOWN_ERROR)
    , dtx_transport(nullptr)
    , dtx_connection(nullptr)
    , dtx_channel()
    , afc(nullptr)
    , house_arrest(nullptr)
    , installer(nullptr)
    , syslog(nullptr)
    , debugger(nullptr)
    , diagnostics(nullptr)
    , screenshot(nullptr)
    , mounter(nullptr)
{
}

bool DeviceClient::StartService(const QStringList& service_ids)
{
    if (service_ids.isEmpty())
        return false;

    QString service_id;
    lockdownd_error = lockdownd_error_t::LOCKDOWN_E_UNKNOWN_ERROR;
    for (const auto& svc_id : std::as_const(service_ids))
    {
        service_id = svc_id;
        lockdownd_error = lockdownd_start_service(client, svc_id.toUtf8().data(), &services[service_id]);
        if (lockdownd_error == LOCKDOWN_E_SUCCESS)
            return true;
    }
    qDebug() << "Start lockdownd service" << service_ids.join(", ") << service_id << "error :" << lockdownd_error;
    return false;
}

DeviceClient::DeviceClient(QString udid, QStringList service_ids, QStringList service_ids_2) : DeviceClient()
{
    device_error = idevice_new_with_options(&device, udid.toStdString().c_str(), idevice_options(IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK));
    if (device_error != idevice_error_t::IDEVICE_E_SUCCESS) {
        qDebug() << "New device error : " << device_error;
        return;
    }

    lockdownd_error = lockdownd_client_new_with_handshake(device, &client, TOOL_NAME);
    if (lockdownd_error != lockdownd_error_t::LOCKDOWN_E_SUCCESS) {
        qDebug() << "New lockdownd client error : " << lockdownd_error;
        return;
    }

    if (!StartService(service_ids))
        return;

    if (!StartService(service_ids_2))
        return;
}

DeviceClient::DeviceClient(RemoteAddress address, QStringList service_ids, QStringList service_ids_2) : DeviceClient()
{
    device_error = idevice_new_remote(&device, address.ipAddress.toStdString().c_str(), address.port);
    if (device_error != idevice_error_t::IDEVICE_E_SUCCESS) {
        qDebug() << "New device error : " << device_error;
        return;
    }

    lockdownd_error = lockdownd_client_new_with_handshake_remote(device, &client, TOOL_NAME);
    if (lockdownd_error != lockdownd_error_t::LOCKDOWN_E_SUCCESS) {
        qDebug() << "New lockdownd client error : " << lockdownd_error;
        return;
    }

    if (!StartService(service_ids))
        return;

    if (!StartService(service_ids_2))
        return;
}

DeviceClient::~DeviceClient()
{
    // instruments
    if (dtx_channel)
    {
        dtx_channel = nullptr;
    }
    if (dtx_connection)
    {
        delete dtx_connection;
        dtx_connection = nullptr;
    }
    if (dtx_transport)
    {
        delete dtx_transport;
        dtx_transport = nullptr;
    }

    // services
    if (screenshot)
    {
        screenshotr_client_free(screenshot);
        screenshot = nullptr;
    }

    if (mounter)
    {
        mobile_image_mounter_hangup(mounter);
        mobile_image_mounter_free(mounter);
        mounter = nullptr;
    }

    if (diagnostics) {
        diagnostics_relay_goodbye(diagnostics);
        diagnostics_relay_client_free(diagnostics);
        diagnostics = nullptr;
    }

    if (afc)
    {
        afc_client_free(afc);
        afc = nullptr;
    }

    if (house_arrest)
    {
        house_arrest_client_free(house_arrest);
        house_arrest = nullptr;
    }

    if (installer)
    {
        instproxy_client_free(installer);
        installer = nullptr;
    }

    if (syslog)
    {
        syslog_relay_stop_capture(syslog);
        syslog_relay_client_free(syslog);
        syslog = nullptr;
    }

    if (debugger)
    {
        debugserver_client_free(debugger);
        debugger = nullptr;
    }

    // basics
    for (auto& service : services.keys())
    {
        lockdownd_service_descriptor_free(services[service]);
    }
    services.clear();

    if (client)
    {
        lockdownd_client_free(client);
        client = nullptr;
    }

    if (device)
    {
        idevice_free(device);
        device = nullptr;
    }
}
