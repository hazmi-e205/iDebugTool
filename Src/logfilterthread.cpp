#include "logfilterthread.h"

LogFilterThread::LogFilterThread()
    : m_maxCachedLogs(0)
    , m_paddings({0,0,0,0})
    , m_terminateFilter(false)
    , m_processLogs(true)
{
    m_thread = new QThread();
    connect(m_thread, SIGNAL(started()), SLOT(doWork()));
    moveToThread(m_thread);
}

LogFilterThread::~LogFilterThread()
{
    delete m_thread;
}

void LogFilterThread::LogsFilterByString(QString text_or_regex)
{
    if (m_thread->isRunning())
        StopFilter();

    m_currentFilter = text_or_regex;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_thread->start();
}

void LogFilterThread::LogsExcludeByString(QString exclude_text)
{
    if (m_thread->isRunning())
        StopFilter();

    m_excludeFilter = exclude_text;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_thread->start();
}

void LogFilterThread::LogsFilterByPID(QString pid_name)
{
    if (m_thread->isRunning())
        StopFilter();

    if (pid_name.trimmed().toLower().contains("by user apps only") || pid_name.trimmed().toLower().contains("related to user apps"))
    {
        m_pidFilter = m_pidlist[pid_name.trimmed().toLower()];
    }
    else
    {
        m_pidFilter = pid_name;
    }
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_thread->start();
}

void LogFilterThread::SystemLogsFilter(QString text_or_regex, QString pid_name, QString exclude_text)
{
    if (m_thread->isRunning())
        StopFilter();

    if (pid_name.trimmed().toLower().contains("by user apps only") || pid_name.trimmed().toLower().contains("related to user apps"))
    {
        m_pidFilter = m_pidlist[pid_name.trimmed().toLower()];
    }
    else
    {
        m_pidFilter = pid_name;
    }
    m_currentFilter = text_or_regex;
    m_excludeFilter = exclude_text;
    m_paddings = {0,0,0,0};
    m_oldFiltered.clear();

    m_logsWillBeFiltered = m_cachedLogs;
    m_thread->start();
}

void LogFilterThread::ReloadLogsFilter()
{
    SystemLogsFilter(m_currentFilter, m_pidFilter, m_excludeFilter);
}

void LogFilterThread::UpdateInstalledList(QMap<QString, QJsonDocument> applist)
{
    QStringList userBinaries;
    foreach (auto appinfo, applist)
    {
        QString bin_name = appinfo["CFBundleExecutable"].toString();
        userBinaries << bin_name;
    }
    m_pidlist["by user apps only"] = userBinaries.join("\\[|") + "\\[";
    m_pidlist["related to user apps"] = userBinaries.join("|");
    ReloadLogsFilter();
}

QList<int> LogFilterThread::ParsePaddings(LogPacket log)
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

QString LogFilterThread::LogToString(LogPacket log)
{
    QList<int> lengths = ParsePaddings(log);
    auto lines = log.getLogMessage().split('\n');
    if (lines.count() > 0)
    {
        QString result;
        for (qsizetype line_idx = 0; line_idx < lines.count(); line_idx++)
        {
            if (line_idx > 0)
                result.append(QString("%1\t%2\t%3\t%4\t%5")
                        .arg("", -lengths[0])
                        .arg("", -lengths[1])
                        .arg("", -lengths[2])
                        .arg("", -lengths[3])
                        .arg(lines[line_idx]));
            else
                result.append(QString("%1\t%2\t%3\t%4\t%5")
                        .arg(log.getDateTime(), -lengths[0])
                        .arg(log.getDeviceName(), -lengths[1])
                        .arg(log.getProcessID(), -lengths[2])
                        .arg(log.getLogType(), -lengths[3])
                        .arg(lines[line_idx]));
        }
        return result;
    }
    return log.GetRawData();
}

void LogFilterThread::StopFilter()
{
    m_mutex.lock();
    m_terminateFilter = true;
    m_mutex.unlock();
}

void LogFilterThread::doWork()
{
    foreach (LogPacket log, m_logsWillBeFiltered)
    {
        if (m_terminateFilter) {
            m_terminateFilter = false;
            break;
        }

        if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, ""))
            m_oldFiltered.append(LogToString(log) + "\r");
    }
    m_thread->quit();

    if (!m_processLogs) {
        emit FilterComplete(m_oldFiltered);
        m_oldFiltered.clear();
    }
}

void LogFilterThread::UpdateSystemLog(LogPacket log)
{
    if (!m_processLogs)
        return;

    m_cachedLogs.append(log);
    if (m_cachedLogs.count() > m_maxCachedLogs)
    {
        qsizetype deleteCount = m_cachedLogs.count() - m_maxCachedLogs;
        m_cachedLogs.remove(0, deleteCount);
    }

    if (m_thread->isRunning())
    {
        m_newFiltered.append(LogToString(log) + "\r");
    }
    else
    {
        QString compiled;
        if (!m_oldFiltered.isEmpty()) {
            compiled.append(m_oldFiltered);
            m_oldFiltered.clear();
        }
        if (!m_newFiltered.isEmpty()) {
            compiled.append(m_newFiltered);
            m_newFiltered.clear();
        }
        if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, "")) {
            if (!compiled.isEmpty())
                compiled.append(LogToString(log));
            else
                compiled = LogToString(log);

            emit FilterComplete(compiled);
        }
    }
}
