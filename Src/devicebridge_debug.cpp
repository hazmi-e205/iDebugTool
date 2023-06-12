#include "devicebridge.h"

static int quit_flag = 0;
static int cancel_receive()
{
    return quit_flag;
}

static debugserver_error_t debugserver_client_handle_response(debugserver_client_t client, char** response, int* exit_status)
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
        printf("%s", o);
        fflush(stdout);
    } else if (r[0] == 'T') {
        /* thread stopped information */
        printf("Thread stopped. Details:\n%s", r + 1);
        if (exit_status != NULL) {
            /* "Thread stopped" seems to happen when assert() fails.
               Use bash convention where signals cause an exit
               status of 128 + signal
            */
            *exit_status = 128 + SIGABRT;
        }
        /* Break out of the loop. */
        dres = DEBUGSERVER_E_UNKNOWN_ERROR;
    } else if (r[0] == 'E') {
        printf("ERROR: %s\n", r + 1);
    } else if (r[0] == 'W' || r[0] == 'X') {
        /* process exited */
        debugserver_decode_string(r + 1, strlen(r) - 1, &o);
        if (o != NULL) {
            printf("Exit %s: %u\n", (r[0] == 'W' ? "status" : "due to signal"), o[0]);
            if (exit_status != NULL) {
                /* Use bash convention where signals cause an
                   exit status of 128 + signal
                */
                *exit_status = o[0] + (r[0] == 'W' ? 0 : 128);
            }
        } else {
            printf("Unable to decode exit status from %s", r);
            dres = DEBUGSERVER_E_UNKNOWN_ERROR;
                }
    } else if (r && strlen(r) == 0) {
        printf("empty response");
    } else {
        printf("ERROR: unhandled response '%s'", r);
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
    QString container;
    if (m_installedApps.contains(bundleId))
        container = m_installedApps[bundleId]["Container"].toString();
    else
        return;

    /* start and connect to debugserver */
    if (debugserver_client_start_service(m_device, &m_debugger, TOOL_NAME) != DEBUGSERVER_E_SUCCESS) {
        fprintf(stderr,
            "Could not start com.apple.debugserver!\n"
            "Please make sure to mount the developer disk image first:\n"
            "  1) Get the iOS version from `ideviceinfo -k ProductVersion`.\n"
            "  2) Find the matching iPhoneOS DeveloperDiskImage.dmg files.\n"
            "  3) Run `ideviceimagemounter` with the above path.\n");
        return;
    }

    /* set receive params */
    if (debugserver_client_set_receive_params(m_debugger, cancel_receive, 250) != DEBUGSERVER_E_SUCCESS) {
        fprintf(stderr, "Error in debugserver_client_set_receive_params\n");
        debugserver_client_free(m_debugger);
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
            debugserver_client_handle_response(m_debugger, &response, NULL);
            return;
        }
        free(response);
        response = NULL;
    }*/

    /* set maximum packet size */
    printf("Setting maximum packet size...");
    char* packet_size[2] = { (char*)"1024", NULL};
    debugserver_command_new("QSetMaxPacketSize:", 1, packet_size, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
            debugserver_client_free(m_debugger);
            return;
        }
        free(response);
        response = NULL;
    }

    /* set working directory */
    printf("Setting working directory...");
    char* working_dir[2] = {container.toUtf8().data(), NULL};
    debugserver_command_new("QSetWorkingDir:", 1, working_dir, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
            debugserver_client_free(m_debugger);
            return;
        }
        free(response);
        response = NULL;
    }

    /* set environment */
    QStringList environtment = parameters.split(" ");
    printf("Setting environment...");
    foreach (const QString& env, environtment) {
        printf("setting environment variable: %s", env.toUtf8().data());
        debugserver_client_set_environment_hex_encoded(m_debugger, env.toUtf8().data(), NULL);
    }

    /* set arguments and run app */
    printf("Setting argv...");
    QString path = m_installedApps[bundleId]["Container"].toString() + "/" + m_installedApps[bundleId]["CFBundleExecutable"].toString();
    QStringList args = QStringList() << path << arguments.split(" ");
    char **app_argv = (char**)malloc(sizeof(char*) * args.count());
    int idx = 0;
    foreach (const QString& env, environtment) {
        printf("app_argv[%d] = %s", idx, env.toUtf8().data());
        app_argv[idx] = env.toUtf8().data();
        idx += 1;
    }
    app_argv[args.count()] = NULL;
    debugserver_client_set_argv(m_debugger, args.count(), app_argv, NULL);
    free(app_argv);

    /* check if launch succeeded */
    printf("Checking if launch succeeded...");
    debugserver_command_new("qLaunchSuccess", 0, NULL, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
            debugserver_client_free(m_debugger);
            return;
        }
        free(response);
        response = NULL;
    }

    int res = 0;
    if (detach_after_start) {
        printf("Detaching from app");
        debugserver_command_new("D", 0, NULL, &command);
        dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
        debugserver_command_free(command);
        command = NULL;

        res = (dres == DEBUGSERVER_E_SUCCESS) ? 0: -1;
        debugserver_client_free(m_debugger);
        return;
    }

    /* set thread */
    printf("Setting thread...");
    debugserver_command_new("Hc0", 0, NULL, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
            debugserver_client_free(m_debugger);
            return;
        }
        free(response);
        response = NULL;
    }

    /* continue running process */
    printf("Continue running process...");
    debugserver_command_new("c", 0, NULL, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    printf("Continue response: %s", response);

    /* main loop which is parsing/handling packets during the run */
    printf("Entering run loop...");
    while (!quit_flag) {
        if (dres != DEBUGSERVER_E_SUCCESS) {
            printf("failed to receive response; error %d", dres);
            break;
        }

        if (response) {
            printf("response: %s", response);
            if (strncmp(response, "OK", 2) != 0) {
                dres = debugserver_client_handle_response(m_debugger, &response, &res);
                if (dres != DEBUGSERVER_E_SUCCESS) {
                    printf("failed to process response; error %d; %s", dres, response);
                    break;
                }
            }
        }
        if (res >= 0) {
            debugserver_client_free(m_debugger);
            return;
        }

        dres = debugserver_client_receive_response(m_debugger, &response, NULL);
    }

    /* ignore quit_flag after this point */
    if (debugserver_client_set_receive_params(m_debugger, NULL, 5000) != DEBUGSERVER_E_SUCCESS) {
        fprintf(stderr, "Error in debugserver_client_set_receive_params\n");
        debugserver_client_free(m_debugger);
        return;
    }

    /* interrupt execution */
    debugserver_command_new("\x03", 0, NULL, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
        }
        free(response);
        response = NULL;
    }

    /* kill process after we finished */
    printf("Killing process...");
    debugserver_command_new("k", 0, NULL, &command);
    dres = debugserver_client_send_command(m_debugger, command, &response, NULL);
    debugserver_command_free(command);
    command = NULL;
    if (response) {
        if (strncmp(response, "OK", 2) != 0) {
            debugserver_client_handle_response(m_debugger, &response, NULL);
        }
        free(response);
        response = NULL;
    }
    debugserver_client_free(m_debugger);
}

