#include "devicebridge.h"
#include "asyncmanager.h"

using namespace instruments;

void DeviceBridge::StartDebugging(QString bundleId, bool detach_after_start, QString parameters, QString arguments)
{
    AsyncManager::Get()->StartAsyncRequest([this, bundleId, detach_after_start, parameters, arguments]()
    {
        if (!m_installedApps.contains(bundleId)) {
            emit DebuggerReceived("App not installed yet", true);
            return;
        }

        QString appPath = m_installedApps[bundleId]["Path"].toString()
                        + "/" + m_installedApps[bundleId]["CFBundleExecutable"].toString();
        QString container = m_installedApps[bundleId]["Container"].toString();

        MobileOperation op = MobileOperation::DEBUGGER;
        if (!CreateClient(op))
            return;

        m_clients[op]->instrument = Instruments::Create(m_clients[op]->device, m_clients[op]->client);
        if (!m_clients[op]->instrument) {
            emit DebuggerReceived("Failed to create Instruments connection", true);
            RemoveClient(op);
            return;
        }

        // Build env map and args list from the space-separated parameter/argument strings
        std::map<std::string, std::string> env;
        for (const QString& param : parameters.split(" ", Qt::SkipEmptyParts)) {
            int sep = param.indexOf('=');
            if (sep > 0)
                env[param.left(sep).toStdString()] = param.mid(sep + 1).toStdString();
        }

        std::vector<std::string> args;
        for (const QString& arg : arguments.split(" ", Qt::SkipEmptyParts))
            args.push_back(arg.toStdString());

        Error err = m_clients[op]->instrument->Debug().LaunchWithDebugger(
            bundleId.toStdString(),
            appPath.toStdString(),
            container.toStdString(),
            args,
            env,
            detach_after_start,
            [this](const std::string& line) {
                emit DebuggerReceived(QString::fromStdString(line));
            },
            [this](Error, const std::string& msg) {
                // DebugLoop has already freed the debugserver client internally.
                // Just signal the UI — do NOT call RemoveClient here, as that
                // would destroy the Instruments/DebuggerService object from
                // inside its own background thread (join-self deadlock).
                emit DebuggerReceived(QString::fromStdString(msg), true);
            }
        );

        if (err != Error::Success) {
            emit DebuggerReceived(
                "Could not start com.apple.debugserver!\n"
                "Please make sure to mount the developer disk image first:\n"
                "  1) Go to Image Mounter in Toolbox.\n"
                "  2) Choose closest image version to your iOS version.\n"
                "  3) Click Download and Mount.", true);
            RemoveClient(op);
        }
    });
}

void DeviceBridge::StopDebugging()
{
    if (!m_clients.contains(MobileOperation::DEBUGGER))
        return;

    auto& inst = m_clients[MobileOperation::DEBUGGER]->instrument;
    if (inst)
        inst->Debug().Stop(); // blocks until DebugLoop thread exits

    // Thread is done — safe to destroy the client now
    RemoveClient(MobileOperation::DEBUGGER);
}

void DeviceBridge::ClearDebugger()
{
    m_debugHandler->ClearCachedLogs();
}

void DeviceBridge::SetMaxDebuggerLogs(qsizetype number)
{
    m_debugHandler->SetMaxCachedLogs(number);
}

void DeviceBridge::DebuggerFilterByString(QString text_or_regex)
{
    m_debugHandler->LogsFilterByString(text_or_regex);
}

void DeviceBridge::DebuggerExcludeByString(QString exclude_text)
{
    m_debugHandler->LogsExcludeByString(exclude_text);
}

void DeviceBridge::DebuggerFilter(QString text_or_regex, QString exclude_text)
{
    m_debugHandler->LogsFilter(text_or_regex, exclude_text);
}

void DeviceBridge::DebuggerReloadFilter()
{
    m_debugHandler->ReloadLogsFilter();
}
