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

void DeviceBridge::ClearCachedLogs()
{
    m_logHandler->ClearCachedLogs();
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
