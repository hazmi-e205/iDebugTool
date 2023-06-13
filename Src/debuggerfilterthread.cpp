#include "debuggerfilterthread.h"
#include <QThread>
#include "utils.h"

DebuggerFilterThread::DebuggerFilterThread()
    : m_maxCachedLogs(0)
    , m_terminateFilter(false)
    , m_thread(new QThread())
    , m_processLogs(true)
{
    connect(m_thread, SIGNAL(started()), SLOT(doWork()));
    moveToThread(m_thread);
}

DebuggerFilterThread::~DebuggerFilterThread()
{
    ClearCachedLogs();
    delete m_thread;
}

void DebuggerFilterThread::ClearCachedLogs()
{
    m_oldFiltered.clear();
    m_newFiltered.clear();
    m_cachedLogs.clear();
    m_logsWillBeFiltered.clear();
}

void DebuggerFilterThread::LogsFilterByString(QString text_or_regex)
{
    if (m_thread->isRunning())
        StopFilter();

    m_currentFilter = text_or_regex;
    StartFilter();
}

void DebuggerFilterThread::LogsExcludeByString(QString exclude_text)
{
    if (m_thread->isRunning())
        StopFilter();

    m_excludeFilter = exclude_text;
    StartFilter();
}

void DebuggerFilterThread::LogsFilter(QString text_or_regex, QString exclude_text)
{
    m_currentFilter = text_or_regex;
    m_excludeFilter = exclude_text;
    StartFilter();
}

void DebuggerFilterThread::ReloadLogsFilter()
{
    LogsFilter(m_currentFilter, m_excludeFilter);
}

void DebuggerFilterThread::UpdateLog(QString log)
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
        if (Filter(log, m_currentFilter, m_excludeFilter))
            m_newFiltered.append(log + "\r\n");
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

        if (Filter(log, m_currentFilter, m_excludeFilter))
            compiled.append(log);

        if (!compiled.isEmpty())
            emit FilterComplete(compiled.trimmed());
    }
}

void DebuggerFilterThread::StartFilter()
{
    m_logsWillBeFiltered = m_cachedLogs;
    m_oldFiltered.clear();

    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    m_thread->start();
}

void DebuggerFilterThread::StopFilter()
{
    m_mutex.lock();
    m_terminateFilter = true;
    m_mutex.unlock();
}

bool DebuggerFilterThread::Filter(QString raw_string, QString text_or_regex, QString exclude_text)
{
    bool isPassed = true;

    //filter text or regex
    if (!text_or_regex.isEmpty())
    {
        isPassed = raw_string.contains(text_or_regex, Qt::CaseInsensitive)
                || !FindRegex(raw_string, text_or_regex).isEmpty();
    }

    //exclude text or regex
    if (!exclude_text.isEmpty())
    {
        isPassed = isPassed
                && !(raw_string.contains(exclude_text, Qt::CaseInsensitive)
                     || !FindRegex(raw_string, exclude_text).isEmpty());
    }

    return isPassed;
}

void DebuggerFilterThread::doWork()
{
    emit FilterStatusChanged(true);
    foreach (const QString& log, m_logsWillBeFiltered)
    {
        if (m_terminateFilter) {
            m_terminateFilter = false;
            break;
        }
        if (Filter(log, m_currentFilter, m_excludeFilter))
            m_oldFiltered.append(log + "\r\n");
    }
    m_thread->quit();
    m_thread->wait();
    emit FilterStatusChanged(false);

    if (!m_processLogs) {
        emit FilterComplete(m_oldFiltered);
        m_oldFiltered.clear();
    }
}
