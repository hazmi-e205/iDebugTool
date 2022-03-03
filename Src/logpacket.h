#ifndef LOGPACKET_H
#define LOGPACKET_H

#include <QString>

class LogPacket
{
public:
    LogPacket();
    LogPacket(QString rawString);
    LogPacket(QString detaTime, QString deviceName, QString processID, QString logType, QString logMessage);

    void Parse(QString rawString);
    QString GetRawData();
    bool Filter(QString text_or_regex, QString pid_name, QString exclude_text);
    bool IsHeader(QString rawString);
    bool IsEmpty();
    void Clear();

    QString getDateTime   () { return m_DateTime   ; }
    QString getDeviceName () { return m_DeviceName ; }
    QString getProcessID  () { return m_ProcessID  ; }
    QString getLogType    () { return m_LogType    ; }
    QString getLogMessage () { return m_LogMessage ; }

    void setDateTime   (QString detaTime   ) { m_DateTime    = detaTime   ; }
    void setDeviceName (QString deviceName ) { m_DeviceName  = deviceName ; }
    void setProcessID  (QString processID  ) { m_ProcessID   = processID  ; }
    void setLogType    (QString logType    ) { m_LogType     = logType    ; }
    void setLogMessage (QString logMessage ) { m_LogMessage  = logMessage ; }

private:
    void SetRegexValue();
    QString m_DateTime, m_DeviceName, m_ProcessID, m_LogType, m_LogMessage;
    QString m_dateRegex, m_devRegex, m_appRegex, m_typeRegex, m_headRegex;
};

#endif // LOGPACKET_H
