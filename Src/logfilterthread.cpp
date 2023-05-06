#include "logfilterthread.h"

LogFilterThread::LogFilterThread()
    : m_maxCachedLogs(0)
    , m_paddings({0,0,0,0})
    , m_terminateFilter(false)
    , m_thread(new QThread())
    , m_processLogs(true)
{
    connect(m_thread, SIGNAL(started()), SLOT(doWork()));
    moveToThread(m_thread);
}

LogFilterThread::~LogFilterThread()
{
    ClearCachedLogs();
    delete m_thread;
}

void LogFilterThread::ClearCachedLogs()
{
    m_oldFiltered.clear();
    m_newFiltered.clear();
    m_cachedLogs.clear();
    m_logsWillBeFiltered.clear();
}

void LogFilterThread::LogsFilterByString(QString text_or_regex)
{
    if (m_thread->isRunning())
        StopFilter();

    m_currentFilter = text_or_regex;
    StartFilter();
}

void LogFilterThread::LogsExcludeByString(QString exclude_text)
{
    if (m_thread->isRunning())
        StopFilter();

    m_excludeFilter = exclude_text;
    StartFilter();
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
    StartFilter();
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
    StartFilter();
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
                m_paddings[idx] = log.getDateTime().size();
            lengths << m_paddings[idx];
            break;
        case 1:
            if (m_paddings[idx] < log.getDeviceName().size())
                m_paddings[idx] = log.getDeviceName().size();
            lengths << m_paddings[idx];
            break;
        case 2:
            if (m_paddings[idx] < log.getProcessID().size())
                m_paddings[idx] = log.getProcessID().size();
            lengths << m_paddings[idx];
            break;
        case 3:
            if (m_paddings[idx] < log.getLogType().size())
                m_paddings[idx] = log.getLogType().size();
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

void LogFilterThread::StartFilter()
{
    m_paddings = {0,0,0,0};
    m_logsWillBeFiltered = m_cachedLogs;
    m_oldFiltered.clear();

    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    m_thread->start();
}

void LogFilterThread::StopFilter()
{
    m_mutex.lock();
    m_terminateFilter = true;
    m_mutex.unlock();
}

void LogFilterThread::doWork()
{
    emit FilterStatusChanged(true);
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
    m_thread->wait();
    emit FilterStatusChanged(false);

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
        if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, ""))
            m_newFiltered.append(LogToString(log) + "\r");
    }
    else
    {
        QString compiled;
        if (!m_oldFiltered.isEmpty()) {
#ifdef DEBUG
            compiled.append(":: START CACHED FILTERRED :::::::::::::\r");
#endif
            compiled.append(m_oldFiltered);
#ifdef DEBUG
            compiled.append(":: END CACHED FITLTERRED  :::::::::::::\r");
#endif
            m_oldFiltered.clear();
        }
        if (!m_newFiltered.isEmpty()) {
#ifdef DEBUG
            compiled.append(":: START FILTERRED DURING FILTERING :::::::::::::\r");
#endif
            compiled.append(m_newFiltered);
#ifdef DEBUG
            compiled.append(":: END FILTERRED DURING FILTERING   :::::::::::::\r");
#endif
            m_newFiltered.clear();
        }

        if (log.Filter(m_currentFilter, m_pidFilter, m_excludeFilter, ""))
            compiled.append(LogToString(log));

        if (!compiled.isEmpty())
            emit FilterComplete(compiled.trimmed());
    }
}
