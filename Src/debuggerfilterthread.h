#ifndef DEBUGGERFILTERTHREAD_H
#define DEBUGGERFILTERTHREAD_H

#include <QMutex>
#include <QObject>

class DebuggerFilterThread : public QObject
{
    Q_OBJECT
public:
    DebuggerFilterThread();
    ~DebuggerFilterThread();

    inline void SetMaxCachedLogs(qsizetype number) { m_maxCachedLogs = number; }
    void ClearCachedLogs();
    void LogsFilterByString(QString text_or_regex);
    void LogsExcludeByString(QString exclude_text);
    void LogsFilter(QString text_or_regex, QString exclude_text);
    void ReloadLogsFilter();
    void UpdateLog(QString log);

private:
    void StartFilter();
    void StopFilter();
    bool Filter(QString raw_string, QString text_or_regex, QString exclude_text);

    QList<QString> m_cachedLogs, m_logsWillBeFiltered;
    qsizetype m_maxCachedLogs;
    QString m_oldFiltered, m_newFiltered;
    QString m_currentFilter, m_excludeFilter;
    bool m_terminateFilter;
    QThread *m_thread;
    QMutex m_mutex;
    bool m_processLogs;
signals:
    void FilterComplete(QString compiledLogs);
    void FilterStatusChanged(bool isfiltering);

private slots:
    void doWork();
};

#endif // DEBUGGERFILTERTHREAD_H
