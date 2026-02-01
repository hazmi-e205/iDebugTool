#ifndef DEVICECLIENT_H
#define DEVICECLIENT_H

#include <QStringList>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include "libimobiledevice/afc.h"
#include "libimobiledevice/debugserver.h"
#include "libimobiledevice/diagnostics_relay.h"
#include "libimobiledevice/house_arrest.h"
#include "libimobiledevice/installation_proxy.h"
#include "libimobiledevice/mobile_image_mounter.h"
#include "libimobiledevice/screenshotr.h"
#include "libimobiledevice/syslog_relay.h"
#include "idevice/instrument/dtxconnection.h"
#include "idevice/instrument/dtxtransport.h"
#include "utils.h"

using namespace idevice;

class DeviceClient
{
public:
    DeviceClient(QString udid, QStringList service_ids = QStringList(), QStringList service_ids_2 = QStringList());
    DeviceClient(RemoteAddress address, QStringList service_ids = QStringList(), QStringList service_ids_2 = QStringList());
    ~DeviceClient();

    idevice_t device;
    lockdownd_client_t client;
    QMap<QString,lockdownd_service_descriptor_t> services;

    lockdownd_error_t lockdownd_error;
    idevice_error_t device_error;

    DTXTransport* dtx_transport;
    DTXConnection* dtx_connection;
    std::unique_ptr<DTXChannel> dtx_channel;

    afc_client_t afc;
    house_arrest_client_t house_arrest;
    instproxy_client_t installer;
    syslog_relay_client_t syslog;
    debugserver_client_t debugger;
    diagnostics_relay_client_t diagnostics;
    screenshotr_client_t screenshot;
    mobile_image_mounter_client_t mounter;

private:
    DeviceClient();
};

#endif // DEVICECLIENT_H
