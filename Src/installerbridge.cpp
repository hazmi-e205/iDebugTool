#include "devicebridge.h"
#include <QDebug>
#include <QMessageBox>
#include <zip.h>
#include <libgen.h>
#include <stdio.h>
#include "utils.h"

QJsonArray DeviceBridge::GetInstalledApps()
{
    StartInstaller();

    QJsonArray jsonArray;
    plist_t client_opts = instproxy_client_options_new();
    instproxy_client_options_add(client_opts, "ApplicationType", "User", nullptr);

    plist_t apps = nullptr;
    instproxy_error_t err = instproxy_browse(m_installer, client_opts, &apps);
    if (err != INSTPROXY_E_SUCCESS || !apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
        QMessageBox::critical(m_mainWidget, "Error", "ERROR: instproxy_browse returnd an invalid plist!", QMessageBox::Ok);
        return jsonArray;
    }

    jsonArray = PlistToJsonArray(apps);
    plist_free(apps);
    return jsonArray;
}

void DeviceBridge::UninstallApp(QString bundleId)
{
    StartInstaller();
    instproxy_uninstall(m_installer, bundleId.toUtf8(), NULL, InstallerCallback, NULL);
}

void DeviceBridge::InstallApp(InstallMode cmd, QString path)
{
    StartInstaller();
    StartAFC();

    char *bundleidentifier = NULL;
    plist_t sinf = NULL;
    plist_t meta = NULL;
    QString pkgname = "";
    struct stat fst;
    uint64_t af = 0;
    char buf[8192];

    char **strs = NULL;
    if (afc_get_file_info(m_afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
        if (afc_make_directory(m_afc, PKG_PATH) != AFC_E_SUCCESS) {
            QMessageBox::warning(m_mainWidget, "Warning", "WARNING: Could not create directory '" + QString(PKG_PATH) + "' on device!", QMessageBox::Ok);
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
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: zip_open: " + path + ": " + QString::number(errp), QMessageBox::Ok);
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
                    QMessageBox::critical(m_mainWidget, "Error", "ERROR: Can't open afc://" + dstpath + " for writing.", QMessageBox::Ok);
                    zip_fclose(zfile);
                    continue;
                }

                struct zip_stat zs;
                zip_stat_init(&zs);
                if (zip_stat_index(zf, i, 0, &zs) != 0) {
                    QMessageBox::critical(m_mainWidget, "Error", "ERROR: zip_stat_index " + QString::number(i) + " failed!", QMessageBox::Ok);
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
                                QMessageBox::critical(m_mainWidget, "Error", "ERROR: AFC Write error!", QMessageBox::Ok);
                                break;
                            }
                            total += written;
                        }
                        if (total != amount) {
                            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Wrote only " + QString::number(total) + " of " + QString::number(amount), QMessageBox::Ok);
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
    } else if (S_ISDIR(fst.st_mode)) {
        /* upload developer app directory */
        instproxy_client_options_add(client_opts, "PackageType", "Developer", NULL);

        pkgname = QString(PKG_PATH) + "/" + basename(path.toUtf8().data());

        printf("Uploading %s package contents... ", basename(path.toUtf8().data()));
        afc_upload_dir(m_afc, path, pkgname);
        printf("DONE.\n");

        /* extract the CFBundleIdentifier from the package */

        /* construct full filename to Info.plist */
        QString filename = path + "/Info.plist";

        struct stat st;
        FILE *fp = NULL;

        if (stat(filename.toUtf8().data(), &st) == -1 || (fp = fopen(filename.toUtf8().data(), "r")) == NULL) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not locate " + filename + " in app!", QMessageBox::Ok);
            return;
        }
        size_t filesize = st.st_size;
        char *ibuf = (char*)malloc(filesize * sizeof(char));
        size_t amount = fread(ibuf, 1, filesize, fp);
        if (amount != filesize) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not read " + QString::number(filesize) + " bytes from " + filename, QMessageBox::Ok);
            return;
        }
        fclose(fp);

        plist_t info = NULL;
        if (memcmp(ibuf, "bplist00", 8) == 0) {
            plist_from_bin(ibuf, filesize, &info);
        } else {
            plist_from_xml(ibuf, filesize, &info);
        }
        free(ibuf);

        if (!info) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not parse Info.plist!", QMessageBox::Ok);
            return;
        }

        plist_t bname = plist_dict_get_item(info, "CFBundleIdentifier");
        if (bname) {
            plist_get_string_val(bname, &bundleidentifier);
        }
        plist_free(info);
        info = NULL;
    } else {
        zf = zip_open(path.toUtf8().data(), 0, &errp);
        if (!zf) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: zip_open: " + path + ": " + QString::number(errp), QMessageBox::Ok);
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
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Unable to locate app directory in archive!", QMessageBox::Ok);
            return;
        }

        /* construct full filename to Info.plist */
        filename = app_directory_name + "Info.plist";

        if (zip_get_contents(zf, filename.toUtf8().data(), 0, &zbuf, &len) < 0) {
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not locate " + filename + " in archive!", QMessageBox::Ok);
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
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not parse Info.plist!", QMessageBox::Ok);
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
            QMessageBox::critical(m_mainWidget, "Error", "ERROR: Could not determine value for CFBundleExecutable!", QMessageBox::Ok);
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

        printf("Copying '%s' to device... ", path.toUtf8().data());

        if (afc_upload_file(m_afc, path, pkgname) < 0) {
            return;
        }

        printf("DONE.\n");

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
        printf("Installing '%s'\n", bundleidentifier);
        instproxy_install(m_installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
    } else {
        printf("Upgrading '%s'\n", bundleidentifier);
        instproxy_upgrade(m_installer, pkgname.toUtf8().data(), client_opts, InstallerCallback, NULL);
    }
    instproxy_client_options_free(client_opts);
}

void DeviceBridge::InstallerCallback(plist_t command, plist_t status, void *unused)
{
}
