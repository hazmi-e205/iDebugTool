#include "devicebridge.h"

#include "nskeyedarchiver/kavalue.hpp"
#include "nskeyedarchiver/kaarray.hpp"
#include <QJsonDocument>
#include <QJsonArray>

using namespace idevice;

QStringList DeviceBridge::GetAttributes(AttrType type)
{
    QStringList list;
    if (!m_connection) {
        m_transport = new DTXTransport(m_device, false);
        m_connection = new DTXConnection(m_transport);
        m_connection->Connect();
    }

    auto channel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.deviceinfo");
    std::string selector = std::string("sysmon") + (type == AttrType::PROCESS ? "Process" : "System") + "Attributes";
    std::shared_ptr<DTXMessage> message = DTXMessage::CreateWithSelector(selector.c_str());
    auto response = channel->SendMessageSync(message);
    if (response->PayloadObject()) {
        QJsonDocument availableOpt = QJsonDocument::fromJson(response->PayloadObject()->ToJson().c_str());
        foreach (const auto& value, availableOpt.array())
            list.append(value.toString(""));
    }
    channel->Cancel();
    return list;
}

void DeviceBridge::StartMonitor(unsigned int interval_ms, QStringList system_attr, QStringList process_attr)
{
    if (!m_connection) {
        m_transport = new DTXTransport(m_device, false);
        m_connection = new DTXConnection(m_transport);
        m_connection->Connect();
    }

    nskeyedarchiver::KAArray proccessAttrs(nskeyedarchiver::KAArray("NSSet", {"NSSet", "NSObject"}));
    nskeyedarchiver::KAArray systemAttrs(nskeyedarchiver::KAArray("NSSet", {"NSSet", "NSObject"}));

    foreach (const auto& value, process_attr)
        proccessAttrs.push_back(nskeyedarchiver::KAValue(value.toUtf8().data()));

    foreach (const auto& value, system_attr)
        systemAttrs.push_back(nskeyedarchiver::KAValue(value.toUtf8().data()));

    m_sysmontapChannel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.sysmontap");
    auto message = DTXMessage::CreateWithSelector("setConfig:");
    nskeyedarchiver::KAMap configs(nskeyedarchiver::KAMap("NSMutableDictionary", {"NSMutableDictionary", "NSDictionary", "NSObject"}));
    configs["ur"] = interval_ms;
    configs["sampleInterval"] = interval_ms * 1000000;
    configs["bm"] = 0;
    configs["cpuUsage"] = true;
    configs["procAttrs"] = proccessAttrs;
    configs["sysAttrs"] = systemAttrs;
    message->AppendAuxiliary(nskeyedarchiver::KAValue(configs));
    auto response = m_sysmontapChannel->SendMessageSync(message);
    response->Dump();

    m_sysmontapChannel->SetMessageHandler([=](std::shared_ptr<DTXMessage> msg) {
        if (msg->PayloadObject()) {
            qDebug() << msg->PayloadObject()->ToJson().c_str();
        }
    });

    message = DTXMessage::CreateWithSelector("start");
    response = m_sysmontapChannel->SendMessageSync(message);
    response->Dump();
}

void DeviceBridge::StopMonitor()
{
    if (m_sysmontapChannel) {
        auto message = DTXMessage::CreateWithSelector("stop");
        m_sysmontapChannel->SendMessageSync(message);
        m_sysmontapChannel->Cancel();
    }
}

void DeviceBridge::GetProcessList()
{
    if (!m_connection) {
        m_transport = new DTXTransport(m_device, false);
        m_connection = new DTXConnection(m_transport);
        m_connection->Connect();
    }

    auto channel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.deviceinfo");
    std::shared_ptr<DTXMessage> message = DTXMessage::CreateWithSelector("runningProcesses");
    auto response = channel->SendMessageSync(message);
    if (response->PayloadObject()) {
        qDebug() << response->PayloadObject()->ToJson().c_str();
    }
    channel->Cancel();
}

void DeviceBridge::StartFPS(unsigned int interval_ms)
{
    if (!m_connection) {
        m_transport = new DTXTransport(m_device, false);
        m_connection = new DTXConnection(m_transport);
        m_connection->Connect();
    }

    m_openglChannel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.graphics.opengl");
    auto message = DTXMessage::CreateWithSelector("availableStatistics");
    m_openglChannel->SendMessageSync(message);

    message = DTXMessage::CreateWithSelector("driverNames");
    m_openglChannel->SendMessageSync(message);

    message = DTXMessage::CreateWithSelector("setSamplingRate:");
    message->AppendAuxiliary(nskeyedarchiver::KAValue((float)interval_ms / 100.f));
    m_openglChannel->SendMessageSync(message);

    m_openglChannel->SetMessageHandler([=](std::shared_ptr<DTXMessage> msg) {
        if (msg->PayloadObject()) {
            qDebug() << msg->PayloadObject()->ToJson().c_str();
        }
    });

    message = DTXMessage::CreateWithSelector("startSamplingAtTimeInterval:");
    message->AppendAuxiliary(nskeyedarchiver::KAValue(0.0f));
    auto response = m_openglChannel->SendMessageSync(message);
    response->Dump();
}

void DeviceBridge::StopFPS()
{
    if (m_openglChannel){
        auto message = DTXMessage::CreateWithSelector("stopSampling");
        m_openglChannel->SendMessageSync(message);
        m_openglChannel->Cancel();
    }
}
