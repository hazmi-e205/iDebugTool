#include "devicebridge.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <zip.h>
#include <libgen.h>
#include <stdio.h>
#include "utils.h"
#include "asyncmanager.h"
#include "common/json.h"

QJsonDocument DeviceBridge::GetInstalledApps()
{
    QJsonDocument jsonArray;
    if (!m_installer) {
        return jsonArray;
    }

    plist_t client_opts = instproxy_client_options_new();
    instproxy_client_options_add(client_opts, "ApplicationType", "User", nullptr);

    plist_t apps = nullptr;
    instproxy_error_t err = instproxy_browse(m_installer, client_opts, &apps);
    if (err != INSTPROXY_E_SUCCESS || !apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: instproxy_browse returnd an invalid plist!");
        return jsonArray;
    }

    jsonArray = PlistToJson(apps);
    plist_free(apps);
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
        m_installedApps = newAppList;
    };

    if (doAsync)
    {
        AsyncManager::Get()->StartAsyncRequest([&apps_update]() {
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
    AsyncManager::Get()->StartAsyncRequest([this, bundleId]() {
        if (!m_installer) {
            return;
        }
        instproxy_uninstall(m_installer, bundleId.toUtf8().data(), NULL, InstallerCallback, NULL);
    });
}

void DeviceBridge::InstallApp(InstallerMode cmd, QString path)
{
    AsyncManager::Get()->StartAsyncRequest([this, cmd, path]() {
        if (!m_installer) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: instproxy_client_private is null!\nPlease connect your device to this PC!");
            return;
        }
        char *bundleidentifier = NULL;
        plist_t sinf = NULL;
        plist_t meta = NULL;
        QString pkgname = "";
        uint64_t af = 0;
        char buf[8192];

        char **strs = NULL;
        if (afc_get_file_info(m_afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
            if (afc_make_directory(m_afc, PKG_PATH) != AFC_E_SUCCESS) {
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

        /* open install package */
        int errp = 0;
        struct zip *zf = NULL;

        if ((path.length() > 5) && (path.endsWith(".ipcc", Qt::CaseInsensitive))) {
            zf = zip_open(path.toUtf8().data(), 0, &errp);
            if (!zf) {
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: zip_open: " + path + ": " + QString::number(errp));
                return;
            }

            char* ipcc = path.toUtf8().data();
            pkgname = QString(PKG_PATH) + "/" + basename(ipcc);
            afc_make_directory(m_afc, pkgname.toUtf8().data());

            printf("Uploading %s package contents... ", basename(ipcc));

            /* extract the contents of the .ipcc file to PublicStaging/<name>.ipcc directory */
            zip_uint64_t numzf = zip_get_num_entries(zf, 0);
            zip_uint64_t i = 0;
            for (i = 0; numzf > 0 && i < numzf; i++) {
                const char* zname = zip_get_name(zf, i, 0);
                QString dstpath;
                if (!zname) continue;
                if (zname[strlen(zname)-1] == '/') {
                    // directory
                    dstpath = pkgname + "/" + zname;
                    afc_make_directory(m_afc, dstpath.toUtf8().data());
                } else {
                    // file
                    struct zip_file* zfile = zip_fopen_index(zf, i, 0);
                    if (!zfile) continue;

                    dstpath = pkgname + "/" + zname;
                    if (afc_file_open(m_afc, dstpath.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) {
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
                        zip_int64_t amount = zip_fread(zfile, buf, sizeof(buf));
                        if (amount == 0) {
                            break;
                        }

                        if (amount > 0) {
                            uint32_t written, total = 0;
                            while (total < amount) {
                                written = 0;
                                if (afc_file_write(m_afc, af, buf, amount, &written) != AFC_E_SUCCESS) {
                                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: AFC Write error!");
                                    break;
                                }
                                total += written;
                            }
                            if (total != amount) {
                                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Wrote only " + QString::number(total) + " of " + QString::number(amount));
                                afc_file_close(m_afc, af);
                                zip_fclose(zfile);
                                return;
                            }
                        }

                        zfsize += amount;
                    }

                    afc_file_close(m_afc, af);
                    af = 0;

                    zip_fclose(zfile);
                }
            }
            free(ipcc);
            printf("DONE.\n");

            instproxy_client_options_add(client_opts, "PackageType", "CarrierBundle", NULL);
        } else if (QFileInfo(path).isDir()) {
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
            if (!afc_upload_dir(m_afc, path, pkgname, afc_callback))
            {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not send " + path);
                return;
            }
            instproxy_client_options_add(client_opts, "PackageType", "Developer", NULL);
        } else {
            zf = zip_open(path.toUtf8().data(), 0, &errp);
            if (!zf) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: zip_open: " + path + ": " + QString::number(errp));
                return;
            }

            /* extract iTunesMetadata.plist from package */
            char *zbuf = NULL;
            uint32_t len = 0;
            plist_t meta_dict = NULL;
            if (zip_get_contents(zf, ITUNES_METADATA_PLIST_FILENAME, 0, &zbuf, &len) == 0) {
                meta = plist_new_data(zbuf, len);
                if (memcmp(zbuf, "bplist00", 8) == 0) {
                    plist_from_bin(zbuf, len, &meta_dict);
                } else {
                    plist_from_xml(zbuf, len, &meta_dict);
                }
            } else {
                fprintf(stderr, "WARNING: could not locate %s in archive!\n", ITUNES_METADATA_PLIST_FILENAME);
            }
            free(zbuf);

            /* determine .app directory in archive */
            zbuf = NULL;
            len = 0;
            plist_t info = NULL;
            QString filename = NULL;
            QString app_directory_name;

            if (zip_get_app_directory(zf, app_directory_name)) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Unable to locate app directory in archive!");
                return;
            }

            /* construct full filename to Info.plist */
            filename = app_directory_name + "Info.plist";

            if (zip_get_contents(zf, filename.toUtf8().data(), 0, &zbuf, &len) < 0) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not locate " + filename + " in archive!");
                zip_unchange_all(zf);
                zip_close(zf);
                return;
            }

            if (memcmp(zbuf, "bplist00", 8) == 0) {
                plist_from_bin(zbuf, len, &info);
            } else {
                plist_from_xml(zbuf, len, &info);
            }
            free(zbuf);

            if (!info) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not parse Info.plist!");
                zip_unchange_all(zf);
                zip_close(zf);
                return;
            }

            char *bundleexecutable = NULL;

            plist_t bname = plist_dict_get_item(info, "CFBundleExecutable");
            if (bname) {
                plist_get_string_val(bname, &bundleexecutable);
            }

            bname = plist_dict_get_item(info, "CFBundleIdentifier");
            if (bname) {
                plist_get_string_val(bname, &bundleidentifier);
            }
            plist_free(info);
            info = NULL;

            if (!bundleexecutable) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, "", 100, "ERROR: Could not determine value for CFBundleExecutable!");
                zip_unchange_all(zf);
                zip_close(zf);
                return;
            }

            QString sinfname = "Payload/" + QString(bundleexecutable) + ".app/SC_Info/" + bundleexecutable + ".sinf";
            free(bundleexecutable);

            /* extract .sinf from package */
            zbuf = NULL;
            len = 0;
            if (zip_get_contents(zf, sinfname.toUtf8().data(), 0, &zbuf, &len) == 0) {
                sinf = plist_new_data(zbuf, len);
            } else {
                fprintf(stderr, "WARNING: could not locate %s in archive!\n", sinfname.toUtf8().data());
            }
            free(zbuf);

            /* copy archive to device */
            pkgname = QString(PKG_PATH) + "/" + bundleidentifier;

            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 0, "Sending " + QFileInfo(path).fileName());
            auto callback = [&](uint32_t uploaded_bytes, uint32_t total_bytes)
            {
                int percentage = int((float(uploaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
                QString message = "Sending " + BytesToString(uploaded_bytes) + " of " + BytesToString(total_bytes);
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, percentage, message);
            };
            int result = afc_upload_file(m_afc, path, pkgname, callback);
            if (result != 0) {
                emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 100, QString::asprintf("ERROR: Failed to send %s : afc error code %d", path.toUtf8().data(), result));
                return;
            }

            if (bundleidentifier) {
                instproxy_client_options_add(client_opts, "CFBundleIdentifier", bundleidentifier, NULL);
            }
            if (sinf) {
                instproxy_client_options_add(client_opts, "ApplicationSINF", sinf, NULL);
            }
            if (meta) {
                instproxy_client_options_add(client_opts, "iTunesMetadata", meta, NULL);
            }
        }
        if (zf) {
            zip_unchange_all(zf);
            zip_close(zf);
        }

        /* perform installation or upgrade */
        if (cmd == CMD_INSTALL) {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 51, "Installing " + QString(bundleidentifier));
            instproxy_install(m_installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
        } else {
            emit InstallerStatusChanged(InstallerMode::CMD_INSTALL, bundleidentifier, 51, "Upgrading " + QString(bundleidentifier));
            instproxy_upgrade(m_installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
        }
        instproxy_client_options_free(client_opts);
    });
}

void DeviceBridge::TriggetInstallerStatus(QJsonDocument command, QJsonDocument status)
{
    InstallerMode pCommand = command["Command"].toString() == "Install" ? InstallerMode::CMD_INSTALL : InstallerMode::CMD_UNINSTALL;
    QString pBundleId = pCommand == InstallerMode::CMD_INSTALL ? command["ClientOptions"]["CFBundleIdentifier"].toString() : command["ApplicationIdentifier"].toString();

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
        percentage = pMessage == "Complete" ? 100 : (50 + status["PercentComplete"].toInt() / 2);
    }
    emit InstallerStatusChanged(pCommand, pBundleId, percentage, pMessage);
}

void DeviceBridge::InstallerCallback(plist_t command, plist_t status, void *unused)
{
    DeviceBridge::Get()->TriggetInstallerStatus(PlistToJson(command), PlistToJson(status));
}
