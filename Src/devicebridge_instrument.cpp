#include "devicebridge.h"
#include "nskeyedarchiver/kavalue.hpp"
#include "nskeyedarchiver/kamap.hpp"
#include "nskeyedarchiver/kaarray.hpp"
#include <QJsonDocument>
#include <QJsonArray>

using namespace idevice;

QStringList DeviceBridge::GetAttributes(AttrType type)
{
    MobileOperation op = MobileOperation::GET_ATTRIBUTES;
    QStringList list;

    if (!CreateClient(op))
        return list;

    m_clients[op]->transport = new DTXTransport(m_clients[op]->device, false);
    m_clients[op]->connection = new DTXConnection(m_clients[op]->transport);
    m_clients[op]->connection->Connect();

    auto channel = m_clients[op]->connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.deviceinfo");
    std::string selector = std::string("sysmon") + (type == AttrType::PROCESS ? "Process" : "System") + "Attributes";
    std::shared_ptr<DTXMessage> message = DTXMessage::CreateWithSelector(selector.c_str());
    auto response = channel->SendMessageSync(message);
    if (response->PayloadObject()) {
        QJsonDocument availableOpt = QJsonDocument::fromJson(response->PayloadObject()->ToJson().c_str());
        foreach (const auto& value, availableOpt.array())
            list.append(value.toString(""));
    }
    channel->Cancel();
    RemoveClient(op);
    return list;
}

void DeviceBridge::StartMonitor(unsigned int interval_ms, QStringList system_attr, QStringList process_attr)
{
    MobileOperation op = MobileOperation::SYSTEM_MONITOR;
    if (!CreateClient(op))
        return;

    m_clients[op]->transport = new DTXTransport(m_clients[op]->device, false);
    m_clients[op]->connection = new DTXConnection(m_clients[op]->transport);
    m_clients[op]->connection->Connect();

    nskeyedarchiver::KAArray proccessAttrs(nskeyedarchiver::KAArray("NSSet", {"NSSet", "NSObject"}));
    nskeyedarchiver::KAArray systemAttrs(nskeyedarchiver::KAArray("NSSet", {"NSSet", "NSObject"}));

    foreach (const auto& value, process_attr)
        proccessAttrs.push_back(nskeyedarchiver::KAValue(value.toUtf8().data()));

    foreach (const auto& value, system_attr)
        systemAttrs.push_back(nskeyedarchiver::KAValue(value.toUtf8().data()));

    m_clients[op]->channel = m_clients[op]->connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.sysmontap");
    auto message = DTXMessage::CreateWithSelector("setConfig:");
    nskeyedarchiver::KAMap configs(nskeyedarchiver::KAMap("NSMutableDictionary", {"NSMutableDictionary", "NSDictionary", "NSObject"}));
    configs["ur"] = interval_ms;
    configs["sampleInterval"] = interval_ms * 1000000;
    configs["bm"] = 0;
    configs["cpuUsage"] = true;
    configs["procAttrs"] = proccessAttrs;
    configs["sysAttrs"] = systemAttrs;
    message->AppendAuxiliary(nskeyedarchiver::KAValue(configs));
    auto response = m_clients[op]->channel->SendMessageSync(message);
    response->Dump();

    m_clients[op]->channel->SetMessageHandler([=](std::shared_ptr<DTXMessage> msg) {
        if (msg->PayloadObject()) {
            qDebug() << msg->PayloadObject()->ToJson().c_str();
        }
    });

    message = DTXMessage::CreateWithSelector("start");
    response = m_clients[op]->channel->SendMessageSync(message);
    response->Dump();
}

void DeviceBridge::StopMonitor()
{
    MobileOperation op = MobileOperation::SYSTEM_MONITOR;
    if (m_clients[op]->channel) {
        auto message = DTXMessage::CreateWithSelector("stop");
        m_clients[op]->channel->SendMessageSync(message);
        m_clients[op]->channel->Cancel();
    }
    RemoveClient(op);
}

void DeviceBridge::GetProcessList()
{
    MobileOperation op = MobileOperation::GET_PROCESS;
    if (!CreateClient(op))
        return;

    m_clients[op]->transport = new DTXTransport(m_clients[op]->device, false);
    m_clients[op]->connection = new DTXConnection(m_clients[op]->transport);
    m_clients[op]->connection->Connect();

    m_clients[op]->channel = m_clients[op]->connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.deviceinfo");
    std::shared_ptr<DTXMessage> message = DTXMessage::CreateWithSelector("runningProcesses");
    auto response = m_clients[op]->channel->SendMessageSync(message);
    if (response->PayloadObject()) {
        qDebug() << response->PayloadObject()->ToJson().c_str();
    }
    m_clients[op]->channel->Cancel();
    RemoveClient(op);
}

void DeviceBridge::StartFPS(unsigned int interval_ms)
{
    MobileOperation op = MobileOperation::FPS_MONITOR;
    if (!CreateClient(op))
        return;

    m_clients[op]->transport = new DTXTransport(m_clients[op]->device, false);
    m_clients[op]->connection = new DTXConnection(m_clients[op]->transport);
    m_clients[op]->connection->Connect();

    m_clients[op]->channel = m_clients[op]->connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.graphics.opengl");
    auto message = DTXMessage::CreateWithSelector("availableStatistics");
    m_clients[op]->channel->SendMessageSync(message);

    message = DTXMessage::CreateWithSelector("driverNames");
    m_clients[op]->channel->SendMessageSync(message);

    message = DTXMessage::CreateWithSelector("setSamplingRate:");
    message->AppendAuxiliary(nskeyedarchiver::KAValue((float)interval_ms / 100.f));
    m_clients[op]->channel->SendMessageSync(message);

    m_clients[op]->channel->SetMessageHandler([=](std::shared_ptr<DTXMessage> msg) {
        if (msg->PayloadObject()) {
            qDebug() << msg->PayloadObject()->ToJson().c_str();
        }
    });

    message = DTXMessage::CreateWithSelector("startSamplingAtTimeInterval:");
    message->AppendAuxiliary(nskeyedarchiver::KAValue(0.0f));
    auto response = m_clients[op]->channel->SendMessageSync(message);
    response->Dump();
}

void DeviceBridge::StopFPS()
{
    MobileOperation op = MobileOperation::FPS_MONITOR;
    if (m_clients[op]->channel){
        auto message = DTXMessage::CreateWithSelector("stopSampling");
        m_clients[op]->channel->SendMessageSync(message);
        m_clients[op]->channel->Cancel();
    }
    RemoveClient(op);
}
