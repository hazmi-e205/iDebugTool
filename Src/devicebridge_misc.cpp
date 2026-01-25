#include "devicebridge.h"
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>


void DeviceBridge::StartDiagnostics(DiagnosticsMode mode)
{
    QStringList serviceIds = QStringList() << "com.apple.mobile.diagnostics_relay" << "com.apple.iosdiagnostics.relay";
    m_clients[MobileOperation::DIAGNOSTICS] = m_isRemote ? new DeviceClient(m_remoteAddress, serviceIds) : new DeviceClient(m_currentUdid, serviceIds);
    if (m_clients[MobileOperation::DIAGNOSTICS]->device_error != IDEVICE_E_SUCCESS || m_clients[MobileOperation::DIAGNOSTICS]->lockdownd_error != LOCKDOWN_E_SUCCESS)
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, m_isRemote ? ("ERROR: No device with " + m_remoteAddress.toString()) : ("ERROR: No device with UDID " + m_currentUdid));
        return;
    }

    diagnostics_relay_error_t err = diagnostics_relay_client_new(m_device, m_clients[MobileOperation::DIAGNOSTICS]->service, &m_clients[MobileOperation::DIAGNOSTICS]->diagnostics);
    if (err != DIAGNOSTICS_RELAY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + m_clients[MobileOperation::DIAGNOSTICS]->service_id + " client! " + QString::number(err));
        return;
    }

    switch (mode)
    {
    case CMD_SLEEP:
        if (diagnostics_relay_sleep(m_clients[MobileOperation::DIAGNOSTICS]->diagnostics) == DIAGNOSTICS_RELAY_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_INFO, "Putting device into deep sleep mode.");
        else
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to put device into deep sleep mode.");
        break;
    case CMD_RESTART:
        if (diagnostics_relay_restart(m_clients[MobileOperation::DIAGNOSTICS]->diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_INFO, "Restarting device.");
        else
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to restart device.");
        break;
    case CMD_SHUTDOWN:
        if (diagnostics_relay_shutdown(m_clients[MobileOperation::DIAGNOSTICS]->diagnostics, DIAGNOSTICS_RELAY_ACTION_FLAG_WAIT_FOR_DISCONNECT) == DIAGNOSTICS_RELAY_E_SUCCESS)
            emit MessagesReceived(MessagesType::MSG_INFO, "Shutting down device.");
        else
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to shutdown device.");
        break;
    default:
        break;
    }

    delete m_clients[MobileOperation::DIAGNOSTICS];
    m_clients.remove(MobileOperation::DIAGNOSTICS);
}

void DeviceBridge::Screenshot(QString path)
{
    QStringList serviceIds = QStringList() << SCREENSHOTR_SERVICE_NAME;
    m_clients[MobileOperation::SCREENSHOOT] = m_isRemote ? new DeviceClient(m_remoteAddress, serviceIds) : new DeviceClient(m_currentUdid, serviceIds);
    if (m_clients[MobileOperation::SCREENSHOOT]->device_error != IDEVICE_E_SUCCESS || m_clients[MobileOperation::SCREENSHOOT]->lockdownd_error != LOCKDOWN_E_SUCCESS)
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, m_isRemote ? ("ERROR: No device with " + m_remoteAddress.toString()) : ("ERROR: No device with UDID " + m_currentUdid));
        return;
    }

    screenshotr_error_t err = screenshotr_client_new(m_device, m_clients[MobileOperation::SCREENSHOOT]->service, &m_clients[MobileOperation::SCREENSHOOT]->screenshot);
    if (err != SCREENSHOTR_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + m_clients[MobileOperation::SCREENSHOOT]->service_id + " client! " + QString::number(err));
        return;
    }

    char *imgdata = NULL;
    uint64_t imgsize = 0;
    screenshotr_error_t error = screenshotr_take_screenshot(m_clients[MobileOperation::SCREENSHOOT]->screenshot, &imgdata, &imgsize);
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

    delete m_clients[MobileOperation::SCREENSHOOT];
    m_clients.remove(MobileOperation::SCREENSHOOT);
}
