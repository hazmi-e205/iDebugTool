#include "devicebridge.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QJsonObject>
#include <zip.h>
#include <libgen.h>
#include <stdio.h>
#include "utils.h"
#include "asyncmanager.h"
#include "common/json.h"
#include "extended_plist.h"
#include "extended_zip.h"

QJsonDocument DeviceBridge::GetInstalledApps()
{
    QJsonDocument jsonArray;
    QStringList serviceIds = QStringList() << INSTPROXY_SERVICE_NAME;
    MobileOperation op = MobileOperation::GET_APPS;
    if (!CreateClient(op, serviceIds))
        return jsonArray;

    lockdownd_service_descriptor_t service = GetService(op, serviceIds);
    if (!service)
        return jsonArray;

    instproxy_error_t err = instproxy_client_new(m_clients[op]->device, service, &m_clients[op]->installer);
    if (err != INSTPROXY_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
        return jsonArray;
    }

    plist_t client_opts = instproxy_client_options_new();
    instproxy_client_options_add(client_opts, "ApplicationType", "User", nullptr);

    plist_t apps = nullptr;
    err = instproxy_browse(m_clients[op]->installer, client_opts, &apps);
    if (err != INSTPROXY_E_SUCCESS || !apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: instproxy_browse returnd an invalid plist!");
        return jsonArray;
    }

    jsonArray = PlistToJson(apps);
    plist_free(apps);
    RemoveClient(op);
    return jsonArray;
}

QMap<QString, QJsonDocument> DeviceBridge::GetInstalledApps(bool doAsync)
{
    auto apps_update = [this](){
        QJsonDocument jsonArray = GetInstalledApps();
        QMap<QString, QJsonDocument> newAppList;
        for (int idx = 0; idx < jsonArray.array().count(); idx++)
        {
            QString bundle_id = jsonArray[idx]["CFBundleIdentifier"].toString();
            QJsonDocument app_info;
            app_info.setObject(jsonArray[idx].toObject());
            newAppList[bundle_id] = app_info;
        }
        m_logHandler->UpdateInstalledList(newAppList);
        m_mutex.lock();
        m_installedApps = newAppList;
        m_mutex.unlock();
    };

    if (doAsync)
    {
        AsyncManager::Get()->StartAsyncRequest([apps_update]() {
            apps_update();
        });
    }
    else
    {
        apps_update();
    }
    return m_installedApps;
}

void DeviceBridge::UninstallApp(QString bundleId)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        QStringList serviceIds = QStringList() << INSTPROXY_SERVICE_NAME;
        MobileOperation op = MobileOperation::UNINSTALL_APP;

        if (!CreateClient(op, serviceIds))
            return;

        lockdownd_service_descriptor_t service = GetService(op, serviceIds);
        if (!service)
            return;

        instproxy_error_t err = instproxy_client_new(m_clients[op]->device, service, &m_clients[op]->installer);
        if (err != INSTPROXY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
            return;
        }

        err = instproxy_uninstall(m_clients[op]->installer, bundleId.toUtf8().data(), NULL, InstallerCallback, NULL);
        if (err != INSTPROXY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Uninstall failed " + bundleId + " client! " + QString::number(err));
        }
    });
}

void DeviceBridge::InstallApp(InstallerMode cmd, QString path)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {

        QStringList id_installer = QStringList() << INSTPROXY_SERVICE_NAME;
        QStringList id_afc = QStringList() << AFC_SERVICE_NAME;
        MobileOperation op = MobileOperation::INSTALL_APP;

        if (!CreateClient(op, id_installer, id_afc))
            return;

        auto client = m_clients.value(op);
        if (!client) {
            RemoveClient(op);
            return;
        }
        auto cancel_flag = m_cancelFlags.value(op);
        if (!cancel_flag) {
            RemoveClient(op);
            return;
        }
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        if (should_stop()) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
            RemoveClient(op);
            return;
        }

        lockdownd_service_descriptor_t service_installer = GetService(op, id_installer);
        if (!service_installer) {
            RemoveClient(op);
            return;
        }

        lockdownd_service_descriptor_t service_afc = GetService(op, id_afc);
        if (!service_afc) {
            RemoveClient(op);
            return;
        }

        instproxy_error_t err = instproxy_client_new(client->device, service_installer, &client->installer);
        if (err != INSTPROXY_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + id_installer.join(", ") + " client! " + QString::number(err));
            RemoveClient(op);
            return;
        }

        afc_error_t aerr = afc_client_new(client->device, service_afc, &client->afc);
        if (aerr != AFC_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + id_afc.join(", ") + " client! " + QString::number(aerr));
            RemoveClient(op);
            return;
        }

        install_app(client->installer, client->afc, cmd, path, should_stop);
    });
}

void DeviceBridge::install_app(instproxy_client_t& installer, afc_client_t &afc, InstallerMode cmd, QString path, std::function<bool()> should_stop)
{
    char *bundleidentifier = NULL;
    QString pkgname = "";
    uint64_t af = 0;
    char buf[8192];

    if (should_stop && should_stop()) {
        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
        return;
    }

    char **strs = NULL;
    if (afc_get_file_info(afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
        if (afc_make_directory(afc, PKG_PATH) != AFC_E_SUCCESS) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 0, "WARNING: Could not create directory '" + QString(PKG_PATH) + "' on device!");
        }
    }
    if (strs) {
        int i = 0;
        while (strs[i]) {
            free(strs[i]);
            i++;
        }
        free(strs);
    }

    plist_t client_opts = instproxy_client_options_new();
    if ((path.length() > 5) && (path.endsWith(".ipcc", Qt::CaseInsensitive)))
    {
#ifdef IPCC_SUPPORT
        int errp = 0;
        struct zip *zf = zip_open(path.toUtf8().data(), 0, &errp);
        if (!zf) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: zip_open: " + path + ": " + QString::number(errp));
            return;
        }

        char* ipcc = path.toUtf8().data();
        pkgname = QString(PKG_PATH) + "/" + basename(ipcc);
        afc_make_directory(afc, pkgname.toUtf8().data());

        printf("Uploading %s package contents... ", basename(ipcc));

        /* extract the contents of the .ipcc file to PublicStaging/<name>.ipcc directory */
        zip_uint64_t numzf = zip_get_num_entries(zf, 0);
        zip_uint64_t i = 0;
        for (i = 0; numzf > 0 && i < numzf; i++) {
            if (should_stop && should_stop()) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
                break;
            }

            const char* zname = zip_get_name(zf, i, 0);
            QString dstpath;
            if (!zname) continue;
            if (zname[strlen(zname)-1] == '/') {
                // directory
                dstpath = pkgname + "/" + zname;
                afc_make_directory(afc, dstpath.toUtf8().data());
            } else {
                // file
                struct zip_file* zfile = zip_fopen_index(zf, i, 0);
                if (!zfile) continue;

                dstpath = pkgname + "/" + zname;
                if (afc_file_open(afc, dstpath.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) {
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Can't open afc://" + dstpath + " for writing.");
                    zip_fclose(zfile);
                    continue;
                }

                struct zip_stat zs;
                zip_stat_init(&zs);
                if (zip_stat_index(zf, i, 0, &zs) != 0) {
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: zip_stat_index " + QString::number(i) + " failed!");
                    zip_fclose(zfile);
                    continue;
                }

                zip_uint64_t zfsize = 0;
                while (zfsize < zs.size) {
                    if (should_stop && should_stop()) {
                        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
                        afc_file_close(afc, af);
                        zip_fclose(zfile);
                        return;
                    }

                    zip_int64_t amount = zip_fread(zfile, buf, sizeof(buf));
                    if (amount == 0) {
                        break;
                    }

                    if (amount > 0) {
                        uint32_t written, total = 0;
                        while (total < amount) {
                            if (should_stop && should_stop()) {
                                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
                                afc_file_close(afc, af);
                                zip_fclose(zfile);
                                return;
                            }

                            written = 0;
                            if (afc_file_write(afc, af, buf, amount, &written) != AFC_E_SUCCESS) {
                                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: AFC Write error!");
                                break;
                            }
                            total += written;
                        }
                        if (total != amount) {
                            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Wrote only " + QString::number(total) + " of " + QString::number(amount));
                            afc_file_close(afc, af);
                            zip_fclose(zfile);
                            return;
                        }
                    }

                    zfsize += amount;
                }

                afc_file_close(afc, af);
                af = 0;

                zip_fclose(zfile);
            }
        }
        if (zf) {
            zip_unchange_all(zf);
            zip_close(zf);
        }
        free(ipcc);
        printf("DONE.\n");

        if (should_stop && should_stop()) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
            return;
        }

        instproxy_client_options_add(client_opts, "PackageType", "CarrierBundle", NULL);
#else
        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: IPCC not supported!");
#endif
    }
    else if (QFileInfo(path).isDir())
    {
        /* extract the CFBundleIdentifier from the package */
        /* construct full filename to Info.plist */
        QString filename = path + "/Info.plist";
        JValue jvInfo;
        if (!jvInfo.readPListFile(filename.toUtf8().data()))
        {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not read " + filename);
            return;
        }
        bundleidentifier = strdup(jvInfo["CFBundleIdentifier"].asCString());

        /* upload developer app directory */
        pkgname = QString(PKG_PATH) + "/" + basename(path.toUtf8().data());
        auto afc_callback = [&](int progress, int total, QString messages){
            int percentage = int((float(progress) / (float(total) * 2.f)) * 100.f);
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, percentage, QString::asprintf("(%d/%d) ", progress, total) + messages);
        };
        if (!afc_upload_dir(afc, path, pkgname, afc_callback, should_stop))
        {
            if (should_stop && should_stop()) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "Cancelled");
                return;
            }
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not send " + path);
            return;
        }
        instproxy_client_options_add(client_opts, "PackageType", "Developer", NULL);
    }
    else
    {
        /* determine .app directory in archive */
        QString app_directory_name;
        if (!ZipGetAppDirectory(path, app_directory_name)) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Unable to locate app directory in archive!");
            return;
        }

        /* construct full filename to Info.plist */
        QString filename = app_directory_name + "\\Info.plist";
        std::vector<char> info;
        if (!ZipGetContents(path, filename, info)) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not locate " + filename + " in archive!");
            return;
        }

        JValue jvInfo;
        if (!jvInfo.readPList(&info[0], info.size()))
        {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not parse Info.plist!");
            return;
        }
        bundleidentifier = strdup(jvInfo["CFBundleIdentifier"].asCString());
        if (!jvInfo.has("CFBundleExecutable")) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not determine value for CFBundleExecutable!");
            return;
        }

        /* copy archive to device */
        pkgname = QString(PKG_PATH) + "/" + bundleidentifier;
        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 0, "Sending " + QFileInfo(path).fileName());
        auto callback = [&](uint32_t uploaded_bytes, uint32_t total_bytes)
        {
            int percentage = int((float(uploaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
            QString message = "Sending " + BytesToString(uploaded_bytes) + " of " + BytesToString(total_bytes);
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, percentage, message);
        };
        int result = afc_upload_file(afc, path, pkgname, callback, should_stop);
        if (result != 0) {
            if (should_stop && should_stop()) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 100, "Cancelled");
                return;
            }
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 100, QString::asprintf("ERROR: Failed to send %s : afc error code %d", path.toUtf8().data(), result));
            return;
        }

        if (bundleidentifier) {
            instproxy_client_options_add(client_opts, "CFBundleIdentifier", bundleidentifier, NULL);
        }
    }

    if (should_stop && should_stop()) {
        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 100, "Cancelled");
        return;
    }

    /* perform installation or upgrade */
    if (cmd == CMD_INSTALL) {
        emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 51, "Installing " + QString(bundleidentifier));
        instproxy_install(installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
    } else {
        emit InstallerStatusChanged(InstallerMode::CMD_UPGRADE, bundleidentifier, 51, "Upgrading " + QString(bundleidentifier));
        instproxy_upgrade(installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
    }
    instproxy_client_options_free(client_opts);
}

void DeviceBridge::TriggetInstallerStatus(QJsonDocument command, QJsonDocument status)
{
    InstallerMode pCommand = InstallerMode::CMD_UNINSTALL;
    if (command["Command"].toString() == "Install")
        pCommand = InstallerMode::CMD_INSTALL;
    else if (command["Command"].toString() == "Upgrade")
        pCommand = InstallerMode::CMD_UPGRADE;

    QString pBundleId = pCommand == InstallerMode::CMD_UNINSTALL ? command["ApplicationIdentifier"].toString() : command["ClientOptions"]["CFBundleIdentifier"].toString();

    int percentage = 0;
    QString pMessage = status["Status"].toString();
    StringWithSpaces(pMessage, true);
    if (pMessage.isEmpty())
    {
        pMessage = status["Error"].toString();
        StringWithSpaces(pMessage, true);

        pMessage += "\n" + status["ErrorDescription"].toString();
        percentage = 100;
    }
    else
    {
        if (pMessage == "Complete")
        {
            percentage = 100;
        }
        else
        {
            percentage = 50 + status["PercentComplete"].toInt() / 2;
        }
    }
    emit InstallerStatusChanged(pCommand, pBundleId, percentage, pMessage);
}

void DeviceBridge::InstallerCallback(plist_t command, plist_t status, void *unused)
{
    if (!m_destroyed)
        DeviceBridge::Get()->TriggetInstallerStatus(PlistToJson(command), PlistToJson(status));
}
