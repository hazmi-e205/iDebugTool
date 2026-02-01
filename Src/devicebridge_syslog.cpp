#include "devicebridge.h"

bool ParseSystemLogs(char &in, LogPacket &out)
{
    static QString m_logTemp = "";
    static bool m_newLine = false;
    switch(in){
    case '\0':
    {
        out = LogPacket(m_logTemp);
        m_logTemp = "";
        m_newLine = false;
        return true;
    }
    case '\n':
    {
        if (m_newLine) {
            m_logTemp += in;
        }
        m_newLine = true;
        break;
    }
    default:
        if (m_newLine) {
            m_logTemp += '\n';
            m_newLine = false;
        }
        m_logTemp += in;
        break;
    }
    return false;
}

void DeviceBridge::SetMaxCachedLogs(qsizetype number)
{
    m_logHandler->SetMaxCachedLogs(number);
}

void DeviceBridge::LogsFilterByString(QString text_or_regex)
{
    m_logHandler->LogsFilterByString(text_or_regex);
}

void DeviceBridge::LogsExcludeByString(QString exclude_text)
{
    m_logHandler->LogsExcludeByString(exclude_text);
}

void DeviceBridge::LogsFilterByPID(QString pid_name)
{
    m_logHandler->LogsFilterByPID(pid_name);
}

void DeviceBridge::SystemLogsFilter(QString text_or_regex, QString pid_name, QString exclude_text)
{
    m_logHandler->SystemLogsFilter(text_or_regex,pid_name,exclude_text);
}

void DeviceBridge::ReloadLogsFilter()
{
    m_logHandler->ReloadLogsFilter();
}

void DeviceBridge::CaptureSystemLogs(bool enable)
{
    m_logHandler->CaptureSystemLogs(enable);
}

bool DeviceBridge::IsSystemLogsCaptured()
{
    return m_logHandler->IsSystemLogsCaptured();
}

void DeviceBridge::ClearCachedLogs()
{
    m_logHandler->ClearCachedLogs();
}

void DeviceBridge::StartSyslog()
{
    MobileOperation op = MobileOperation::SYSLOG;
    QStringList serviceIds = QStringList() << SYSLOG_RELAY_SERVICE_NAME;
    if (!CreateClient(op, serviceIds))
        return;

    lockdownd_service_descriptor_t service = GetService(op, serviceIds);
    if (!service)
        return;

    syslog_relay_error_t err = SYSLOG_RELAY_E_UNKNOWN_ERROR;
    err = syslog_relay_client_new(m_clients[op]->device, service, &m_clients[op]->syslog);
    if (err != SYSLOG_RELAY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
        return;
    }

    /* start capturing syslog */
    err = syslog_relay_start_capture_raw(m_clients[op]->syslog, SystemLogsCallback, nullptr);
    if (err != SYSLOG_RELAY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Unable to start capturing syslog.");
        RemoveClient(op);
    }
}

void DeviceBridge::StopSyslog()
{
    RemoveClient(MobileOperation::SYSLOG);
}

QStringList DeviceBridge::GetPIDOptions(QMap<QString, QJsonDocument>& installed_apps)
{
    QStringList result = QStringList() << "By user apps only" << "Related to user apps";
    foreach (auto appinfo, installed_apps)
    {
        QString bundle_id = appinfo["CFBundleExecutable"].toString();
        result << bundle_id + "\\[\\d+\\]";
        result << bundle_id;
    }
    return result;
}

void DeviceBridge::TriggerSystemLogsReceived(LogPacket log)
{
    m_logHandler->UpdateSystemLog(log);
}

void DeviceBridge::SystemLogsCallback(char c, void *user_data)
{
    if (m_destroyed)
        return;

    LogPacket packet;
    if(ParseSystemLogs(c, packet))
    {
        DeviceBridge::Get()->TriggerSystemLogsReceived(packet);
    }
}
