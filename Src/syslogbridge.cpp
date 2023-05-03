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

QList<int> DeviceBridge::ParsePaddings(LogPacket log)
{
    QList<int> lengths;
    for(int idx = 0; idx < m_paddings.count(); idx++)
    {
        switch (idx) {
        case 0:
            if (m_paddings[idx] < log.getDateTime().size())
                lengths << log.getDateTime().size();
            else
                lengths << m_paddings[idx];
            break;
        case 1:
            if (m_paddings[idx] < log.getDeviceName().size())
                lengths << log.getDeviceName().size();
            else
                lengths << m_paddings[idx];
            break;
        case 2:
            if (m_paddings[idx] < log.getProcessID().size())
                lengths << log.getProcessID().size();
            else
                lengths << m_paddings[idx];
            break;
        case 3:
            if (m_paddings[idx] < log.getLogType().size())
                lengths << log.getLogType().size();
            else
                lengths << m_paddings[idx];
            break;
        default:
            break;
        }
    }
    return lengths;
}

QString DeviceBridge::LogToString(LogPacket log)
{
    QList<int> lengths = ParsePaddings(log);
    auto lines = log.getLogMessage().split('\n');
    if (lines.count() > 0)
    {
        for (qsizetype line_idx = 0; line_idx < lines.count(); line_idx++)
        {
            if (line_idx > 0)
                return QString("%1\t%2\t%3\t%4\t%5")
                        .arg("", -lengths[0])
                        .arg("", -lengths[1])
                        .arg("", -lengths[2])
                        .arg("", -lengths[3])
                        .arg(lines[line_idx]);
            else
                return QString("%1\t%2\t%3\t%4\t%5")
                        .arg(log.getDateTime(), -lengths[0])
                        .arg(log.getDeviceName(), -lengths[1])
                        .arg(log.getProcessID(), -lengths[2])
                        .arg(log.getLogType(), -lengths[3])
                        .arg(lines[line_idx]);
        }
    }
    return log.GetRawData();
}

void DeviceBridge::LogsFilterByString(QString text_or_regex)
{
    if (m_filterThread->IsRunning())
        m_filterThread->StopThreads();

    m_currentFilter = text_or_regex;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_filterThread->StartAsyncRequest([this](){
        foreach (LogPacket log, m_logsWillBeFiltered)
        {
            if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
                m_oldFiltered.append(LogToString(log));
        }
    });
}

void DeviceBridge::LogsExcludeByString(QString exclude_text)
{
    if (m_filterThread->IsRunning())
        m_filterThread->StopThreads();

    m_excludeFilter = exclude_text;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_filterThread->StartAsyncRequest([this](){
        foreach (LogPacket log, m_logsWillBeFiltered)
        {
            if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
                m_oldFiltered.append(LogToString(log));
        }
    });
}

void DeviceBridge::LogsFilterByPID(QString pid_name)
{
    if (m_filterThread->IsRunning())
        m_filterThread->StopThreads();

    if (pid_name.trimmed().toLower().contains("by user apps only") || pid_name.trimmed().toLower().contains("related to user apps"))
    {
        m_installedApps = GetInstalledApps(true);
        bool isUserAppsOnly = pid_name.trimmed().toLower().contains("by user apps only");
        QString op1 = isUserAppsOnly ? "\\[|" : "|";
        QString op2 = isUserAppsOnly ? "\\[" : "";
        QStringList userBinaries;
        foreach (auto appinfo, m_installedApps)
        {
            QString bin_name = appinfo["CFBundleExecutable"].toString();
            userBinaries << bin_name;
        }
        m_pidFilter = userBinaries.join(op1) + op2;
    }
    else
    {
        m_pidFilter = pid_name;
    }
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_filterThread->StartAsyncRequest([this](){
        foreach (LogPacket log, m_logsWillBeFiltered)
        {
            if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
                m_oldFiltered.append(LogToString(log));
        }
    });
}

void DeviceBridge::LogsFilterByBinaries(QString user_binaries)
{
    if (m_filterThread->IsRunning())
        m_filterThread->StopThreads();

    m_userbinaries = user_binaries;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_filterThread->StartAsyncRequest([this](){
        foreach (LogPacket log, m_logsWillBeFiltered)
        {
            if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
                m_oldFiltered.append(LogToString(log));
        }
    });
}

void DeviceBridge::SystemLogsFilter(QString text_or_regex, QString pid_name, QString exclude_text, QString user_binaries)
{
    if (m_filterThread->IsRunning())
        m_filterThread->StopThreads();

    m_currentFilter = text_or_regex;
    m_pidFilter = pid_name;
    m_excludeFilter = exclude_text;
    m_userbinaries = user_binaries;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_filterThread->StartAsyncRequest([this](){
        foreach (LogPacket log, m_logsWillBeFiltered)
        {
            if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, m_userbinaries))
                m_oldFiltered.append(LogToString(log));
        }
    });
}

QStringList DeviceBridge::GetPIDFilteringTemplate()
{
    QStringList result = QStringList() << "By user apps only" << "Related to user apps";
    m_installedApps = GetInstalledApps(true);
    foreach (auto appinfo, m_installedApps)
    {
        QString bundle_id = appinfo["CFBundleExecutable"].toString();
        result << bundle_id + "\\[\\d+\\]";
        result << bundle_id;
    }
    return result;
}

void DeviceBridge::TriggerSystemLogsReceived(LogPacket log)
{
    emit SystemLogsReceived(log);

    if (m_cachedLogs.count() > m_maxCachedLogs)
    {
        qsizetype deleteCount = m_cachedLogs.count() - m_maxCachedLogs;
        m_cachedLogs.remove(0, deleteCount);
    }

    if (m_filterThread->IsRunning())
    {
        m_newFiltered.append(LogToString(log));
    }
    else
    {
        if (!m_oldFiltered.isEmpty()) {
            emit SystemLogsReceived2(m_oldFiltered);
            m_oldFiltered.clear();
        }
        if (!m_newFiltered.isEmpty()) {
            emit SystemLogsReceived2(m_newFiltered);
            m_newFiltered.clear();
        }
        emit SystemLogsReceived2(LogToString(log));
    }
}

void DeviceBridge::SystemLogsCallback(char c, void *user_data)
{
    LogPacket packet;
    if(ParseSystemLogs(c, packet))
    {
        DeviceBridge::Get()->TriggerSystemLogsReceived(packet);
    }
}
