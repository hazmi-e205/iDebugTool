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
        [](const SystemMetrics& m) {
            qDebug() << "CPU:" << m.cpuTotalLoad << "% User:" << m.cpuUserLoad
                     << "% Sys:" << m.cpuSystemLoad << "%"
                     << "Net I/O:" << m.netBytesIn << "/" << m.netBytesOut;
        },
        [](const std::vector<ProcessMetrics>& procs) {
            for (const auto& p : procs) {
                if (p.cpuUsage > 0.1) {
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
        [](const FPSData& data) {
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
