#ifndef LOGFILTERTHREAD_H
#define LOGFILTERTHREAD_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QJsonDocument>
#include "logpacket.h"

class LogFilterThread : public QObject
{
    Q_OBJECT
public:
    LogFilterThread();
    ~LogFilterThread();

    inline void CaptureSystemLogs(bool enable) { m_processLogs = enable; }
    inline void SetMaxCachedLogs(qsizetype number) { m_maxCachedLogs = number; }
    void ClearCachedLogs();
    void LogsFilterByString(QString text_or_regex);
    void LogsExcludeByString(QString exclude_text);
    void LogsFilterByPID(QString pid_name);
    void SystemLogsFilter(QString text_or_regex, QString pid_name, QString exclude_text);
    void ReloadLogsFilter();
    void UpdateInstalledList(QMap<QString, QJsonDocument> applist);
    void UpdateSystemLog(LogPacket log);

private:
    QList<int> ParsePaddings(LogPacket log);
    QString LogToString(LogPacket log);
    void StartFilter();
    void StopFilter();

    QList<LogPacket> m_cachedLogs, m_logsWillBeFiltered;
    qsizetype m_maxCachedLogs;
    QString m_oldFiltered, m_newFiltered;
    QString m_currentFilter, m_pidFilter, m_excludeFilter;
    QList<int> m_paddings;
    bool m_terminateFilter;
    QThread *m_thread;
    QMutex m_mutex;
    std::unordered_map<QString,QString> m_pidlist;
    bool m_processLogs;

signals:
    void FilterComplete(QString compiledLogs);
    void FilterStatusChanged(bool isfiltering);

private slots:
    void doWork();
};

#endif // LOGFILTERTHREAD_H
