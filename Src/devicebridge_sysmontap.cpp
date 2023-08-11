#include "devicebridge.h"

void DeviceBridge::StartMonitor()
{
    m_transport = new DTXTransport(m_device);
    m_connection = new DTXConnection(m_transport);
    m_connection->Connect();

    m_channel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.deviceinfo");
    std::shared_ptr<DTXMessage> message = DTXMessage::CreateWithSelector("sysmonProcessAttributes");
    auto response = m_channel->SendMessageSync(message);
    qDebug() << "response start";
    qDebug() << response->PayloadObject()->ToJson().c_str();
    qDebug() << "response end";


    message = DTXMessage::CreateWithSelector("sysmonSystemAttributes");
    response = m_channel->SendMessageSync(message);
    qDebug() << "response start";
    qDebug() << response->PayloadObject()->ToJson().c_str();
    qDebug() << "response end";
    m_channel->Cancel();

    m_channel = m_connection->MakeChannelWithIdentifier("com.apple.instruments.server.services.sysmontap");
    message = DTXMessage::CreateWithSelector("setConfig:");
    nskeyedarchiver::KAMap configs(nskeyedarchiver::KAMap("NSMutableDictionary", {"NSMutableDictionary", "NSDictionary", "NSObject"}));
    int intervals = 1000;
    configs["ur"] = intervals;
    configs["sampleInterval"] = intervals * 1000000;
    configs["bm"] = 0;
    configs["cpuUsage"] = true;
    message->AppendAuxiliary(nskeyedarchiver::KAValue(configs));
    response = m_channel->SendMessageSync(message);

    m_channel->SetMessageHandler([=](std::shared_ptr<DTXMessage> msg) {
        if (msg->PayloadObject()) {
            printf("response sysmontap start");
            response->Dump();
            printf("response sysmontap end");
        }
    });

    message = DTXMessage::CreateWithSelector("start");
    response = m_channel->SendMessageSync(message);
}

void DeviceBridge::StopMonitor()
{
    m_channel->Cancel();
}
