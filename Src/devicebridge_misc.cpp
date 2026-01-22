#include "devicebridge.h"
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>


void DeviceBridge::StartDiagnostics(DiagnosticsMode mode)
{
    QStringList serviceIds = QStringList() << "com.apple.mobile.diagnostics_relay" << "com.apple.iosdiagnostics.relay";
    StartLockdown(!m_diagnostics, m_miscClient, serviceIds, [&, this](QString& service_id, lockdownd_service_descriptor_t& service){
        diagnostics_relay_error_t err = diagnostics_relay_client_new(m_device, service, &m_diagnostics);
        if (err != DIAGNOSTICS_RELAY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
            return;
        }

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
        if (m_diagnostics) {
            diagnostics_relay_client_free(m_diagnostics);
            m_diagnostics = nullptr;
        }
    });
}

void DeviceBridge::Screenshot(QString path)
{
    AsyncManager::Get()->StartAsyncRequest([this, path]() {
        QStringList serviceIds = QStringList() << SCREENSHOTR_SERVICE_NAME;
        StartLockdown(!m_screenshot, m_miscClient, serviceIds, [&, this](QString& service_id, lockdownd_service_descriptor_t& service){
            screenshotr_error_t err = screenshotr_client_new(m_device, service, &m_screenshot);
            if (err != SCREENSHOTR_E_SUCCESS) {
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
                return;
            }

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

            if (m_screenshot) {
                screenshotr_client_free(m_screenshot);
                m_screenshot = nullptr;
            }
        });

    });
}
