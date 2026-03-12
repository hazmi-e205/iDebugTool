#include "devicebridge.h"
#include <QDateTime>
#include <QFileInfo>

namespace {
QString ReadBoundedCString(const char* data, size_t len)
{
    if (!data || len == 0) {
        return QString();
    }

    size_t actualLen = 0;
    while (actualLen < len && data[actualLen] != '\0') {
        actualLen++;
    }
    return QString::fromUtf8(data, static_cast<int>(actualLen));
}

QString TraceLevelToString(uint8_t level)
{
    switch (level) {
    case 0x00:
        return "<Notice>";
    case 0x01:
        return "<Info>";
    case 0x02:
        return "<Debug>";
    case 0x10:
        return "<Error>";
    case 0x11:
        return "<Fault>";
    default:
        return "<Log>";
    }
}
} // namespace

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

bool DeviceBridge::IsSystemLogsCaptured()
{
    return m_logHandler->IsSystemLogsCaptured();
}

void DeviceBridge::ClearCachedLogs()
{
    m_logHandler->ClearCachedLogs();
}

void DeviceBridge::StartSyslog()
{
    StartSystemLogs(SystemLogsRelay::SYSLOG_RELAY_SERVICE);
}

void DeviceBridge::StartOSTrace()
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        StartSystemLogs(SystemLogsRelay::OSTRACE_RELAY_SERVICE);
    });
}

void DeviceBridge::StartSystemLogs(SystemLogsRelay relayType)
{
    StopSyslog();
    StopOSTrace();

    const bool useOSTrace = relayType == SystemLogsRelay::OSTRACE_RELAY_SERVICE;
    MobileOperation op = useOSTrace ? MobileOperation::OSTRACE : MobileOperation::SYSLOG;
    QStringList serviceIds = QStringList() << (useOSTrace ? OSTRACE_SERVICE_NAME : SYSLOG_RELAY_SERVICE_NAME);
    if (!CreateClient(op, serviceIds))
        return;

    lockdownd_service_descriptor_t service = GetService(op, serviceIds);
    if (!service)
        return;

    if (useOSTrace) {
        ostrace_error_t err = ostrace_client_new(m_clients[op]->device, service, &m_clients[op]->ostrace);
        if (err != OSTRACE_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
            return;
        }

        plist_t options = plist_new_dict();
        plist_dict_set_item(options, "Pid", plist_new_int(-1));
        plist_dict_set_item(options, "StreamFlags", plist_new_uint(60));
        plist_dict_set_item(options, "MessageFilter", plist_new_uint(65535));

        err = ostrace_start_activity(m_clients[op]->ostrace, options, OSTraceCallback, nullptr);
        plist_free(options);
        if (err != OSTRACE_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Unable to start capturing os_trace logs.");
            RemoveClient(op);
        }
        return;
    }

    syslog_relay_error_t err = syslog_relay_client_new(m_clients[op]->device, service, &m_clients[op]->syslog);
    if (err != SYSLOG_RELAY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
        return;
    }

    err = syslog_relay_start_capture_raw(m_clients[op]->syslog, SystemLogsCallback, nullptr);
    if (err != SYSLOG_RELAY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Unable to start capturing syslog.");
        RemoveClient(op);
    }
}

void DeviceBridge::StopSyslog()
{
    RemoveClient(MobileOperation::SYSLOG);
}

void DeviceBridge::StopOSTrace()
{
    RemoveClient(MobileOperation::OSTRACE);
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

void DeviceBridge::OSTraceCallback(const void* buf, size_t len, void *user_data)
{
    Q_UNUSED(user_data);

    if (m_destroyed || !buf || len < sizeof(struct ostrace_packet_header_t)) {
        return;
    }

    const auto* hdr = static_cast<const struct ostrace_packet_header_t*>(buf);
    if (hdr->header_size > len) {
        return;
    }

    const size_t payloadLen = len - hdr->header_size;
    const size_t processLen = hdr->procpath_len;
    const size_t imageLen = hdr->imagepath_len;
    const size_t messageLen = hdr->message_len;
    if (processLen + imageLen + messageLen > payloadLen) {
        return;
    }

    const char* data = static_cast<const char*>(buf) + hdr->header_size;
    const QString processPath = ReadBoundedCString(data, processLen);
    const QString imagePath = ReadBoundedCString(data + processLen, imageLen);
    const QString message = ReadBoundedCString(data + processLen + imageLen, messageLen);
    if (message.isEmpty()) {
        return;
    }

    const QString processName = QFileInfo(!imagePath.isEmpty() ? imagePath : processPath).fileName();
    const QString processId = QString("%1[%2]").arg(processName.isEmpty() ? "unknown" : processName).arg(hdr->pid);
    const QString level = TraceLevelToString(hdr->level);
    const QString dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(hdr->time_sec), Qt::UTC).toLocalTime().toString("MMM d HH:mm:ss");
    const QString deviceName = DeviceBridge::Get()->GetDeviceInfo()["DeviceName"].toString();

    DeviceBridge::Get()->TriggerSystemLogsReceived(LogPacket(dateTime, deviceName, processId, level, message));
}
