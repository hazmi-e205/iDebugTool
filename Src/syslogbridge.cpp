#include "devicebridge.h"
#include "utils.h"


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

void DeviceBridge::TriggerSystemLogsReceived(LogPacket log)
{
    emit SystemLogsReceived(log);
}

void DeviceBridge::SystemLogsCallback(char c, void *user_data)
{
    LogPacket packet;
    if(ParseSystemLogs(c, packet))
    {
        DeviceBridge::Get()->TriggerSystemLogsReceived(packet);
    }
}
