#include "devicebridge.h"
#include "asyncmanager.h"
#include "qforeach.h"

static int quit_flag = 0;
static int cancel_receive()
{
    return quit_flag;
}

QString s_debuggerTemp = "";
bool ParseDebugger(const QString &in, QStringList &out)
{
    s_debuggerTemp += in;
    if (s_debuggerTemp.contains("\r\n"))
    {
        QStringList splitted = s_debuggerTemp.split("\r\n");
        s_debuggerTemp = splitted.last();
        splitted.removeLast();
        if (splitted.count() > 1) {
            out = splitted;
            return true;
        }
    }
    return false;
}

debugserver_error_t DeviceBridge::DebugServerHandleResponse(debugserver_client_t client, char** response, int* exit_status)
{
    debugserver_error_t dres = DEBUGSERVER_E_SUCCESS;
    char* o = NULL;
    char* r = *response;

    /* Documentation of response codes can be found here:
       https://github.com/llvm/llvm-project/blob/4fe839ef3a51e0ea2e72ea2f8e209790489407a2/lldb/docs/lldb-gdb-remote.txt#L1269
    */

    if (r[0] == 'O') {
        /* stdout/stderr */
        debugserver_decode_string(r + 1, strlen(r) - 1, &o);
        QStringList parsed;
        if (ParseDebugger(o, parsed))
        {
            foreach(const QString& packet, parsed)
                m_debugHandler->UpdateLog(packet);
        }
    } else if (r[0] == 'T') {
        /* thread stopped information */
        qDebug() << QString::asprintf("Thread stopped. Details:\n%s", r + 1);
        /* Break out of the loop. */
        dres = DEBUGSERVER_E_UNKNOWN_ERROR;
    } else if (r[0] == 'E') {
        qDebug() << QString::asprintf("ERROR: %s", r + 1);
    } else if (r[0] == 'W' || r[0] == 'X') {
        /* process exited */
        debugserver_decode_string(r + 1, strlen(r) - 1, &o);
        if (o != NULL) {
            qDebug() << QString::asprintf("Exit %s: %u", (r[0] == 'W' ? "status" : "due to signal"), o[0]);
        } else {
            qDebug() << QString::asprintf("Unable to decode exit status from %s", r);
            dres = DEBUGSERVER_E_UNKNOWN_ERROR;
        }
    } else if (r && strlen(r) == 0) {
        qDebug() << QString::asprintf("empty response");
    } else {
        qDebug() << QString::asprintf("ERROR: unhandled response '%s'", r);
    }

    if (o != NULL) {
        free(o);
        o = NULL;
    }

    free(*response);
    *response = NULL;
    return dres;
}

void DeviceBridge::StartDebugging(QString bundleId, bool detach_after_start, QString parameters, QString arguments)
{
    AsyncManager::Get()->StartAsyncRequest([this, bundleId, detach_after_start, parameters, arguments]()
    {
        QString container;
        if (m_installedApps.contains(bundleId)) {
            container = m_installedApps[bundleId]["Container"].toString();
        }
        else {
            emit DebuggerReceived("App not installed yet", true);
            return;
        }

        /* start and connect to debugserver */
        if (debugserver_client_start_service(m_device, &m_debugger, TOOL_NAME) != DEBUGSERVER_E_SUCCESS) {
            emit DebuggerReceived(
                    "Could not start com.apple.debugserver!\n"
                    "Please make sure to mount the developer disk image first:\n"
                    "  1) Go to Image Mounter in Toolbox.\n"
                    "  2) Choose closest image version to your iOS version.\n"
                    "  3) Click Download and Mount.", true);
            return;
        }

        /* set receive params */
        if (debugserver_client_set_receive_params(m_debugger, cancel_receive, 250) != DEBUGSERVER_E_SUCCESS) {
            emit DebuggerReceived("Error in debugserver_client_set_receive_params", true);
            CloseDebugger();
            return;
        }

        /* enable logging for the session in debug mode */
        debugserver_command_t command = NULL;
        char* response = NULL;
        debugserver_error_t dres;
        /*fprintf(stdout, "Setting logging bitmask...");
        debugserver_command_new("QSetLogging:bitmask=LOG_ALL|LOG_RNB_REMOTE|LOG_RNB_PACKETS;", 0, NULL, &command);
        debugserver_error_t dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
                return;
            }
            free(response);
            response = NULL;
        }*/

        /* set maximum packet size */
        qDebug() << "Setting maximum packet size...";
        char* packet_size[2] = { (char*)"102400", NULL};
        debugserver_command_new("QSetMaxPacketSize:", 1, packet_size, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
                emit DebuggerReceived("Error setting max packet size occurred: " + QString(response), true);
                CloseDebugger();
                return;
            }
            free(response);
            response = NULL;
        }

        /* set working directory */
        qDebug() << "Setting working directory...";
        char* working_dir[2] = {strdup(container.toUtf8().data()), NULL};
        debugserver_command_new("QSetWorkingDir:", 1, working_dir, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
                emit DebuggerReceived("Error setting working directory occurred: " + QString(response), true);
                CloseDebugger();
                return;
            }
            free(response);
            response = NULL;
        }

        /* set environment */
        QStringList environtment = parameters.split(" ");
        qDebug() << "Setting environment...";
        foreach (const QString& env, environtment) {
            qDebug() << QString::asprintf("setting environment variable: %s", env.toUtf8().data());
            debugserver_client_set_environment_hex_encoded(m_debugger, env.toUtf8().data(), NULL);
        }

        /* set arguments and run app */
        qDebug() << "Setting argv...";
        QString path = m_installedApps[bundleId]["Path"].toString() + "/" + m_installedApps[bundleId]["CFBundleExecutable"].toString();
        QStringList args = QStringList() << path << arguments.split(" ");
        char **app_argv = (char**)malloc(sizeof(char*) * (args.count() + 1));
        int idx = 0;
        foreach (const QString& env, args) {
            qDebug() << QString::asprintf("app_argv[%d] = %s", idx, env.toUtf8().data());
            app_argv[idx] = strdup(env.toUtf8().data());
            idx += 1;
        }
        app_argv[args.count()] = NULL;
        debugserver_client_set_argv(m_debugger, args.count(), app_argv, NULL);
        free(app_argv);

        /* check if launch succeeded */
        qDebug() << "Checking if launch succeeded...";
        debugserver_command_new("qLaunchSuccess", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
                emit DebuggerReceived("Error checking if launch succeeded occurred: " + QString(response), true);
                CloseDebugger();
                return;
            }
            free(response);
            response = NULL;
        }

        int res = -1;
        if (detach_after_start) {
            qDebug() << "Detaching from app";
            debugserver_command_new("D", 0, NULL, &command);
            dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
            debugserver_command_free(command);
            command = NULL;

            res = (dres == DEBUGSERVER_E_SUCCESS) ? 0: -1;
            CloseDebugger();
            return;
        }

        /* set thread */
        qDebug() << "Setting thread...";
        debugserver_command_new("Hc0", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
                emit DebuggerReceived("Error setting thread occurred: " + QString(response), true);
                CloseDebugger();
                return;
            }
            free(response);
            response = NULL;
        }

        /* continue running process */
        qDebug() << "Continue running process...";
        debugserver_command_new("c", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        qDebug() << QString::asprintf("Continue response: %s", response);

        /* main loop which is parsing/handling packets during the run */
        qDebug() << "Entering run loop...";
        while (!quit_flag) {
            if (dres != DEBUGSERVER_E_SUCCESS) {
                qDebug() << QString::asprintf("failed to receive response; error %d", dres);
                break;
            }

            if (response) {
                //qDebug() << QString::asprintf("response: %s", response);
                if (strncmp(response, "OK", 2) != 0) {
                    dres = DebugServerHandleResponse(m_debugger, &response, &res);
                    if (dres != DEBUGSERVER_E_SUCCESS) {
                        qDebug() << QString::asprintf("failed to process response; error %d; %s", dres, response);
                        break;
                    }
                }
            }
            if (res >= 0) {
                emit DebuggerReceived("Debugger stopped.", true);
                CloseDebugger();
                return;
            }

            dres = debugserver_client_receive_response(m_debugger, &response, NULL);
        }

        /* ignore quit_flag after this point */
        if (debugserver_client_set_receive_params(m_debugger, NULL, 5000) != DEBUGSERVER_E_SUCCESS) {
            emit DebuggerReceived("Error in debugserver_client_set_receive_params", true);
            CloseDebugger();
            return;
        }

        /* interrupt execution */
        debugserver_command_new("\x03", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
            }
            free(response);
            response = NULL;
        }

        /* kill process after we finished */
        qDebug() << "Killing process...";
        debugserver_command_new("k", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;
        if (response) {
            if (strncmp(response, "OK", 2) != 0) {
                DebugServerHandleResponse(m_debugger, &response, NULL);
            }
            free(response);
            response = NULL;
        }
        CloseDebugger();
        emit DebuggerReceived("Debugger stopped..", true);
    });
}

void DeviceBridge::StopDebugging()
{
    if (m_debugger)
        quit_flag = 1;

    while (quit_flag == 1)
    {
        QThread::sleep(1);
    }
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

void DeviceBridge::CloseDebugger()
{
    quit_flag = 0;
    if (m_debugger)
    {
        debugserver_client_free(m_debugger);
        m_debugger = nullptr;
    }
}
