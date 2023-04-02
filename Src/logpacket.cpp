#include "logpacket.h"
#include "utils.h"

LogPacket::LogPacket()
{
    SetRegexValue();
}

LogPacket::LogPacket(QString rawString)
{
    SetRegexValue();
    Parse(rawString);
}

LogPacket::LogPacket(QString detaTime, QString deviceName, QString processID, QString logType, QString logMessage) :
    m_DateTime(detaTime),
    m_DeviceName(deviceName),
    m_ProcessID(processID),
    m_LogType(logType),
    m_LogMessage(logMessage)
{
    SetRegexValue();
}

void LogPacket::Parse(QString rawString)
{
    QString mHeaderStr = FindRegex(rawString, m_headRegex);

    if (mHeaderStr.isEmpty())
    {
        //another line of log
        if (!IsEmpty())
        {
            m_LogMessage += "\n";
        }
        m_LogMessage += rawString;
    }
    else
    {
        //if header found here
        m_DateTime    = FindRegex(mHeaderStr, m_dateRegex);
        m_ProcessID   = FindRegex(mHeaderStr, m_appRegex);
        m_DeviceName  = FindRegex(mHeaderStr, m_dateRegex + "\\s+" + m_devRegex + "\\s+" + m_appRegex).remove(m_DateTime).remove(m_ProcessID).simplified().remove(' ');
        m_LogType     = FindRegex(mHeaderStr, m_typeRegex);
        m_LogMessage  = rawString.sliced(mHeaderStr.length());
    }
}

QString LogPacket::GetRawData()
{
    return m_DateTime + "\t" + m_ProcessID + "\t" + m_LogType + "\t" + m_LogMessage;
}

bool LogPacket::Filter(QString text_or_regex, QString pid_name, QString exclude_text, QString user_binaries)
{
    bool isPassed = true;

    //filter text or regex
    if (!text_or_regex.isEmpty())
    {
        isPassed = GetRawData().toLower().contains(text_or_regex.toLower())
                || !FindRegex(GetRawData(), text_or_regex).isEmpty();
    }

    //filter pid name
    if (!pid_name.isEmpty())
    {
        isPassed = isPassed &&
                (getProcessID().toLower().contains(pid_name.toLower())
                || !FindRegex(getProcessID(), pid_name).isEmpty());
    }

    //exclude text or regex
    if (!exclude_text.isEmpty())
    {
        isPassed = isPassed &&
                !(GetRawData().toLower().contains(exclude_text.toLower())
                || !FindRegex(GetRawData(), exclude_text).isEmpty());
    }

    //exclude text or regex for system
    if (!user_binaries.isEmpty())
    {
        isPassed = isPassed &&
                (getProcessID().toLower().contains(user_binaries.toLower())
                || !FindRegex(getProcessID(), user_binaries).isEmpty());
    }

    return isPassed;
}

bool LogPacket::IsHeader(QString rawString)
{
    QRegularExpression re(m_headRegex);
    QRegularExpressionMatch match = re.match(rawString);
    return match.hasMatch();
}

bool LogPacket::IsEmpty()
{
    return m_DateTime.isEmpty()
            && m_DeviceName.isEmpty()
            && m_ProcessID.isEmpty()
            && m_LogType.isEmpty()
            && m_LogMessage.isEmpty();
}

void LogPacket::Clear()
{
    m_DateTime      = "";
    m_DeviceName    = "";
    m_ProcessID     = "";
    m_LogType       = "";
    m_LogMessage    = "";
}

void LogPacket::SetRegexValue()
{
    m_dateRegex = "((Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) (( [1-9])|([1-2][0-9])|([3][0-1])) (?:[0-1]\\d|2[0-3]):(?:[0-5]\\d):(?:[0-5]\\d))";
    m_devRegex  = "[\\S]*";
    m_appRegex  = "[\\S]*\\[[0-9]+\\]";
    m_typeRegex = "<[\\S]*>";
    m_headRegex = m_dateRegex + " " + m_devRegex + " " + m_appRegex + " " + m_typeRegex + ": ";
}
