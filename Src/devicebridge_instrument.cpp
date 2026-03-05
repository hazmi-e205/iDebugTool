#include "devicebridge.h"

using namespace instruments;

QStringList DeviceBridge::GetAttributes(AttrType type)
{
    MobileOperation op = MobileOperation::GET_ATTRIBUTES;
    QStringList list;

    if (!CreateClient(op))
        return list;

    m_clients[op]->instrument = Instruments::Create(m_clients[op]->device, m_clients[op]->client);
    if (!m_clients[op]->instrument) {
        qDebug() << "ERROR: Failed to create Instruments connection";
        RemoveClient(op);
        return list;
    }

    std::vector<std::string> attrs;
    Error err;
    if (type == AttrType::PROCESS)
        err = m_clients[op]->instrument->Performance().GetProcessAttributes(attrs);
    else
        err = m_clients[op]->instrument->Performance().GetSystemAttributes(attrs);

    if (err == Error::Success) {
        for (const auto& attr : attrs)
            list.append(QString::fromStdString(attr));
    }

    RemoveClient(op);
    return list;
}

void DeviceBridge::StartMonitor(unsigned int interval_ms, QStringList system_attr, QStringList process_attr)
{
    MobileOperation op = MobileOperation::SYSTEM_MONITOR;
    if (!CreateClient(op))
        return;

    m_clients[op]->instrument = Instruments::Create(m_clients[op]->device, m_clients[op]->client);
    if (!m_clients[op]->instrument) {
        qDebug() << "ERROR: Failed to create Instruments connection";
        RemoveClient(op);
        return;
    }

    PerfConfig config;
    config.sampleIntervalMs = interval_ms;
    for (const auto& value : std::as_const(system_attr))
        config.systemAttributes.push_back(value.toStdString());
    for (const auto& value : std::as_const(process_attr))
        config.processAttributes.push_back(value.toStdString());

    m_clients[op]->instrument->Performance().Start(config,
        [this](const SystemMetrics& m) {
            emit SystemLogsReceived2(QString("SystemMetrics > CPU: %0 | User: %1 | Sys: %2 | Net I: %3 | Net O: %4").arg(m.cpuTotalLoad).arg(m.cpuUserLoad).arg(m.cpuSystemLoad).arg(m.netBytesIn).arg(m.netBytesOut));
            qDebug() << "CPU:" << m.cpuTotalLoad << "% User:" << m.cpuUserLoad
                     << "% Sys:" << m.cpuSystemLoad << "%"
                     << "Net I/O:" << m.netBytesIn << "/" << m.netBytesOut;
        },
        [this](const std::vector<ProcessMetrics>& procs) {
            for (const auto& p : procs) {
                if (p.cpuUsage > 0.1) {
                    emit SystemLogsReceived2(QString("ProcessMetrics > PID: %0 | Name: %1 | CPU: %2 | MEM: %3").arg(p.pid).arg(p.name.c_str()).arg(p.cpuUsage).arg(p.memResident));
                    qDebug() << "PID:" << p.pid << p.name.c_str()
                             << "CPU:" << p.cpuUsage << "% MEM:" << p.memResident;
                }
            }
        }
    );
}

void DeviceBridge::StopMonitor()
{
    MobileOperation op = MobileOperation::SYSTEM_MONITOR;
    if (m_clients[op]->instrument)
        m_clients[op]->instrument->Performance().Stop();
    RemoveClient(op);
}

void DeviceBridge::GetProcessList()
{
    MobileOperation op = MobileOperation::GET_PROCESS;
    if (!CreateClient(op))
        return;

    m_clients[op]->instrument = Instruments::Create(m_clients[op]->device, m_clients[op]->client);
    if (!m_clients[op]->instrument) {
        qDebug() << "ERROR: Failed to create Instruments connection";
        RemoveClient(op);
        return;
    }

    std::vector<ProcessInfo> procs;
    Error err = m_clients[op]->instrument->Process().GetProcessList(procs);
    if (err == Error::Success) {
        for (const auto& p : procs) {
            qDebug() << "PID:" << p.pid
                     << (p.isApplication ? "App" : "Proc")
                     << p.bundleId.c_str()
                     << p.name.c_str();
        }
    } else {
        qDebug() << "ERROR: GetProcessList failed with error:" << static_cast<int>(err);
    }

    RemoveClient(op);
}

void DeviceBridge::StartFPS(unsigned int interval_ms)
{
    MobileOperation op = MobileOperation::FPS_MONITOR;
    if (!CreateClient(op))
        return;

    m_clients[op]->instrument = Instruments::Create(m_clients[op]->device, m_clients[op]->client);
    if (!m_clients[op]->instrument) {
        qDebug() << "ERROR: Failed to create Instruments connection";
        RemoveClient(op);
        return;
    }

    m_clients[op]->instrument->FPS().Start(interval_ms,
        [this](const FPSData& data) {
            emit SystemLogsReceived2(QString("FPS: %0 | GPU: %1").arg(data.fps).arg(data.gpuUtilization));
            qDebug() << "FPS:" << data.fps << "GPU:" << data.gpuUtilization << "%";
        }
    );
}

void DeviceBridge::StopFPS()
{
    MobileOperation op = MobileOperation::FPS_MONITOR;
    if (m_clients[op]->instrument)
        m_clients[op]->instrument->FPS().Stop();
    RemoveClient(op);
}
