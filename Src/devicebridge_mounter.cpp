#include "devicebridge.h"
#include "extended_plist.h"
#include "simplerequest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <errno.h>
#include <libimobiledevice-glue/sha.h>
#include <string.h>
#include <sys/stat.h>

QStringList DeviceBridge::GetMountedImages()
{
    QStringList signatures;
    QStringList serviceIds = QStringList() << MOBILE_IMAGE_MOUNTER_SERVICE_NAME;
    MobileOperation name = MobileOperation::GET_MOUNTED;
    if (!CreateClient(name, serviceIds))
        return signatures;

    lockdownd_service_descriptor_t service = GetService(name, serviceIds);
    if (!service)
        return signatures;

    mobile_image_mounter_error_t err = mobile_image_mounter_new(m_clients[name]->device, service, &m_clients[name]->mounter);
    if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));
        return signatures;
    }

    QStringList os_version = m_deviceInfo[m_currentUdid]["ProductVersion"].toString().split(".");
    int major_version = os_version.isEmpty() ? 0 : os_version[0].toInt();
    QStringList lookup_types;
    if (major_version >= 17) {
        lookup_types << "Personalized" << "Developer";
    } else {
        lookup_types << "Developer";
    }

    QJsonDocument doc;
    plist_t result = nullptr;
    for (const auto &type : lookup_types) {
        err = mobile_image_mounter_lookup_image(m_clients[name]->mounter, type.toUtf8().data(), &result);
        if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            doc = PlistToJson(result);
            auto arr = doc["ImageSignature"].toArray();
            for (int idx = 0; idx < arr.count(); idx++)
            {
                signatures.append(arr[idx].toString());
            }
            break;
        }
        if (result) {
            plist_free(result);
            result = nullptr;
        }
    }
    if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "Error: lookup_image returned " + QString::number(err));
    }
    if (result) {
        plist_free(result);
    }

    RemoveClient(name);
    return signatures;
}

bool DeviceBridge::IsImageMounted()
{
    QStringList mounted = DeviceBridge::Get()->GetMountedImages();
    return !mounted.empty();
}

void DeviceBridge::MountImage(QString image_path, QString signature_path)
{
    MobileOperation name = MobileOperation::MOUNT_IMAGE;
    QStringList ids_mounter = QStringList() << MOBILE_IMAGE_MOUNTER_SERVICE_NAME;
    QStringList ids_afc = QStringList() << AFC_SERVICE_NAME;
    if (!CreateClient(name, ids_mounter, ids_afc))
        return;

    lockdownd_service_descriptor_t service_mounter = GetService(name, ids_mounter);
    if (!service_mounter)
        return;


    lockdownd_service_descriptor_t service_afc = GetService(name, ids_afc);
    if (!service_afc)
        return;

    mobile_image_mounter_error_t err = mobile_image_mounter_new(m_clients[name]->device, service_mounter, &m_clients[name]->mounter);
    if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + ids_mounter.join(", ") + " client! " + QString::number(err));
    }

    afc_error_t aerr = afc_client_new(m_clients[name]->device, service_afc, &m_clients[name]->afc);
    if (aerr != AFC_E_SUCCESS) {
        emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + ids_afc.join(", ") + " client! " + QString::number(aerr));
    }

    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        mount_image(m_clients[name]->mounter, m_clients[name]->afc, image_path, signature_path);
        RemoveClient(name);
    });
}

void DeviceBridge::mount_image(mobile_image_mounter_client_t& mounter, afc_client_t& afc, QString image_path, QString signature_path)
{
    unsigned char *sig = NULL;
    unsigned int sig_length = 0;
    size_t image_size = 0;
    mobile_image_mounter_error_t err = MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
    plist_t result = NULL;
    plist_t mount_options = NULL;
    plist_t build_manifest = NULL;
    plist_t identifiers = NULL;
    unsigned char *manifest = NULL;
    unsigned int manifest_size = 0;
    QByteArray signature_data;
    FILE *f = NULL;
    QString image_path_local = image_path;
    bool is_personalized = false;

    QStringList os_version = m_deviceInfo[m_currentUdid]["ProductVersion"].toString().split(".");
    int major_version = os_version.isEmpty() ? 0 : os_version[0].toInt();
    is_personalized = (major_version >= 17) || QFileInfo(image_path).isDir();

    if (is_personalized) {
        if (!QFileInfo(image_path).isDir()) {
            emit MounterStatusChanged("Error: Personalized Disk Image mount expects a directory as image path.");
            return;
        }

        QString image_dir = image_path;
        QByteArray image_dir_bytes = image_dir.toUtf8();
        char *build_manifest_path = string_build_path(image_dir_bytes.constData(), "BuildManifest.plist", NULL);
        if (plist_read_from_file(build_manifest_path, &build_manifest, NULL) != 0) {
            free(build_manifest_path);
            build_manifest_path = string_build_path(image_dir_bytes.constData(), "Restore", "BuildManifest.plist", NULL);
            if (plist_read_from_file(build_manifest_path, &build_manifest, NULL) == 0) {
                image_dir = QDir(image_dir).filePath("Restore");
            }
        }
        free(build_manifest_path);

        if (!build_manifest) {
            emit MounterStatusChanged("Error: Could not locate BuildManifest.plist inside given disk image path.");
            return;
        }

        mobile_image_mounter_error_t merr = mobile_image_mounter_query_personalization_identifiers(mounter, NULL, &identifiers);
        if (merr != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            emit MounterStatusChanged("Error: Failed to query personalization identifiers: " + QString::number(merr));
            plist_free(build_manifest);
            return;
        }

        unsigned int board_id = (unsigned int)plist_dict_get_uint(identifiers, "BoardId");
        unsigned int chip_id = (unsigned int)plist_dict_get_uint(identifiers, "ChipID");
        plist_t build_identities = plist_dict_get_item(build_manifest, "BuildIdentities");
        plist_array_iter iter = NULL;
        plist_array_new_iter(build_identities, &iter);
        plist_t item = NULL;
        plist_t build_identity = NULL;
        do {
            plist_array_next_item(build_identities, iter, &item);
            if (!item) {
                break;
            }
            unsigned int bi_board_id = (unsigned int)plist_dict_get_uint(item, "ApBoardID");
            unsigned int bi_chip_id = (unsigned int)plist_dict_get_uint(item, "ApChipID");
            if (bi_chip_id == chip_id && bi_board_id == board_id) {
                build_identity = item;
                break;
            }
        } while (item);
        plist_mem_free(iter);

        if (!build_identity) {
            emit MounterStatusChanged("Error: The given disk image is not compatible with the current device.");
            plist_free(build_manifest);
            plist_free(identifiers);
            return;
        }

        plist_t p_tc_path = plist_access_path(build_identity, 4, "Manifest", "LoadableTrustCache", "Info", "Path");
        if (!p_tc_path) {
            emit MounterStatusChanged("Error: Could not determine path for trust cache.");
            plist_free(build_manifest);
            plist_free(identifiers);
            return;
        }
        plist_t p_dmg_path = plist_access_path(build_identity, 4, "Manifest", "PersonalizedDMG", "Info", "Path");
        if (!p_dmg_path) {
            emit MounterStatusChanged("Error: Could not determine path for disk image.");
            plist_free(build_manifest);
            plist_free(identifiers);
            return;
        }

        image_dir_bytes = image_dir.toUtf8();
        char *tc_path = string_build_path(image_dir_bytes.constData(), plist_get_string_ptr(p_tc_path, NULL), NULL);
        unsigned char *trust_cache = NULL;
        uint64_t trust_cache_size = 0;
        if (!buffer_read_from_filename(tc_path, (char**)&trust_cache, &trust_cache_size)) {
            emit MounterStatusChanged("Error: Trust cache does not exist at '" + QString::fromUtf8(tc_path) + "'.");
            free(tc_path);
            plist_free(build_manifest);
            plist_free(identifiers);
            return;
        }
        free(tc_path);

        mount_options = plist_new_dict();
        plist_dict_set_item(mount_options, "ImageTrustCache", plist_new_data((char*)trust_cache, trust_cache_size));
        free(trust_cache);

        char *dmg_path = string_build_path(image_dir_bytes.constData(), plist_get_string_ptr(p_dmg_path, NULL), NULL);
        image_path_local = QString::fromUtf8(dmg_path);
        free(dmg_path);

        f = fopen(image_path_local.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening image file '" + image_path_local + "' : " + strerror(errno));
            plist_free(build_manifest);
            plist_free(identifiers);
            plist_free(mount_options);
            return;
        }

        struct stat fst;
        if (fstat(fileno(f), &fst) != 0) {
            emit MounterStatusChanged("Error: fstat on image file '" + image_path_local + "' : " + strerror(errno));
            fclose(f);
            plist_free(build_manifest);
            plist_free(identifiers);
            plist_free(mount_options);
            return;
        }
        image_size = (size_t)fst.st_size;

        if (!signature_path.isEmpty() && QFileInfo(signature_path).exists()) {
            QFile sig_file(signature_path);
            if (!sig_file.open(QIODevice::ReadOnly)) {
                emit MounterStatusChanged("Error: opening signature file '" + signature_path + "'");
                fclose(f);
                plist_free(build_manifest);
                plist_free(identifiers);
                plist_free(mount_options);
                return;
            }
            signature_data = sig_file.readAll();
            sig_file.close();
            if (signature_data.isEmpty()) {
                emit MounterStatusChanged("Error: Could not read signature from file '" + signature_path + "'");
                fclose(f);
                plist_free(build_manifest);
                plist_free(identifiers);
                plist_free(mount_options);
                return;
            }
            sig = reinterpret_cast<unsigned char*>(signature_data.data());
            sig_length = static_cast<unsigned int>(signature_data.size());
        } else {
            emit MounterStatusChanged("Querying nonce from device...");
            unsigned char* nonce_data = nullptr;
            unsigned int nonce_sz = 0;
            merr = mobile_image_mounter_query_nonce(mounter, "DeveloperDiskImage", &nonce_data, &nonce_sz);
            if (merr != MOBILE_IMAGE_MOUNTER_E_SUCCESS || !nonce_data) {
                emit MounterStatusChanged("Error: Failed to query nonce: " + QString::number(merr));
                fclose(f);
                plist_free(build_manifest);
                plist_free(identifiers);
                plist_free(mount_options);
                return;
            }
            QByteArray nonce(reinterpret_cast<char*>(nonce_data), nonce_sz);
            free(nonce_data);

            uint64_t ecid = m_deviceInfo[m_currentUdid]["UniqueChipID"].toVariant().toULongLong();
            emit MounterStatusChanged("Requesting personalization signature from Apple TSS...");
            signature_data = request_tss_ticket(build_identity, identifiers, nonce, ecid);
            if (signature_data.isEmpty()) {
                fclose(f);
                plist_free(build_manifest);
                plist_free(identifiers);
                plist_free(mount_options);
                return;
            }
            sig = reinterpret_cast<unsigned char*>(signature_data.data());
            sig_length = static_cast<unsigned int>(signature_data.size());
        }
    } else {
        QFile sig_file(signature_path);
        if (!sig_file.open(QIODevice::ReadOnly)) {
            emit MounterStatusChanged("Error: opening signature file '" + signature_path + "' : " + strerror(errno));
            return;
        }
        signature_data = sig_file.readAll();
        sig_file.close();
        if (signature_data.isEmpty()) {
            emit MounterStatusChanged("Error: Could not read signature from file '" + signature_path + "'");
            return;
        }
        sig = reinterpret_cast<unsigned char*>(signature_data.data());
        sig_length = static_cast<unsigned int>(signature_data.size());

        f = fopen(image_path.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening image file '" + image_path + "' : " + strerror(errno));
            return;
        }
        QFileInfo img_info(image_path);
        image_size = static_cast<size_t>(img_info.size());
    }

    // struct stat fst;
    // if (stat(image_path.toUtf8().data(), &fst) != 0) {
    //     emit MounterStatusChanged("Error: stat: '" + image_path + "' : " + strerror(errno));
    //     return;
    // }
    // image_size = fst.st_size;
    // if (stat(signature_path.toUtf8().data(), &fst) != 0) {
    //     emit MounterStatusChanged("Error: stat: '" + signature_path + "' : " + strerror(errno));
    //     return;
    // }

    QString targetname = QString(PKG_PATH) + "/staging.dimage";
    QString mountname = QString(PATH_PREFIX) + "/" + targetname;

    MounterType mount_type = DISK_IMAGE_UPLOAD_TYPE_AFC;
    if (major_version >= 7) {
        mount_type = DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE;
    }

    const char *image_type = is_personalized ? "Personalized" : "Developer";

    switch (mount_type) {
    case DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE:
        emit MounterStatusChanged("Uploading '" + QFileInfo(image_path_local).fileName() + "' to device...");
        err = mobile_image_mounter_upload_image(mounter, image_type, image_size, sig, sig_length, ImageMounterCallback, f);
        if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            QString message("ERROR: Unknown error occurred, can't mount.");
            if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
                message = "ERROR: Device is locked, can't mount. Unlock device and try again.";
            }
            emit MounterStatusChanged("Error: " + message);
            if (f) {
                fclose(f);
            }
            if (mount_options) {
                plist_free(mount_options);
            }
            if (build_manifest) {
                plist_free(build_manifest);
            }
            if (identifiers) {
                plist_free(identifiers);
            }
            if (manifest) {
                free(manifest);
            }
            return;
        }
        break;

    default:
        emit MounterStatusChanged("Uploading " + QFileInfo(image_path_local).fileName() + " --> afc:///" + targetname);
        char **strs = NULL;
        if (afc_get_file_info(afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
            if (afc_make_directory(afc, PKG_PATH) != AFC_E_SUCCESS) {
                emit MounterStatusChanged("WARNING: Could not create directory '" + QString(PKG_PATH) + "' on device!\n");
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

        uint64_t af = 0;
        if ((afc_file_open(afc, targetname.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
            fclose(f);
            emit MounterStatusChanged("Error: afc_file_open on '" + targetname + "' failed!");
            if (mount_options) {
                plist_free(mount_options);
            }
            if (build_manifest) {
                plist_free(build_manifest);
            }
            if (identifiers) {
                plist_free(identifiers);
            }
            if (manifest) {
                free(manifest);
            }
            return;
        }

        char buf[8192];
        size_t amount = 0;
        do {
            amount = fread(buf, 1, sizeof(buf), f);
            if (amount > 0) {
                uint32_t written, total = 0;
                while (total < amount) {
                    written = 0;
                    if (afc_file_write(afc, af, buf + total, amount - total, &written) != AFC_E_SUCCESS) {
                        emit MounterStatusChanged("Error: AFC Write error!");
                        break;
                    }
                    total += written;
                }
                if (total != amount) {
                    emit MounterStatusChanged("Error: wrote only " + QString::number(total) + " of " + QString::number(amount));
                    afc_file_close(afc, af);
                    fclose(f);
                    if (mount_options) {
                        plist_free(mount_options);
                    }
                    if (build_manifest) {
                        plist_free(build_manifest);
                    }
                    if (identifiers) {
                        plist_free(identifiers);
                    }
                    if (manifest) {
                        free(manifest);
                    }
                    return;
                }
            }
        }
        while (amount > 0);

        afc_file_close(afc, af);
        break;
    }
    if (f) {
        fclose(f);
    }
    emit MounterStatusChanged("Image uploaded.");

    emit MounterStatusChanged("Mounting...");
    if (mount_options) {
        err = mobile_image_mounter_mount_image_with_options(
            mounter, mountname.toUtf8().data(), sig, sig_length, image_type, mount_options, &result);
    } else {
        err = mobile_image_mounter_mount_image(
            mounter, mountname.toUtf8().data(), sig, sig_length, image_type, &result);
    }
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
    {
        emit MounterStatusChanged(is_personalized ? "Personalized disk image mounted" : "Developer disk image mounted");
    }
    else
    {
        emit MounterStatusChanged("Error: mount_image returned " + QString::number(err));
    }

    if (result)
    {
        emit MounterStatusChanged(PlistToJson(result).toJson());
        plist_free(result);
    }

    if (mount_options) {
        plist_free(mount_options);
    }
    if (build_manifest) {
        plist_free(build_manifest);
    }
    if (identifiers) {
        plist_free(identifiers);
    }
    if (manifest) {
        free(manifest);
    }
}

ssize_t DeviceBridge::ImageMounterCallback(void *buf, size_t size, void *userdata)
{
    if (!m_destroyed)
        return fread(buf, 1, size, (FILE*)userdata);
}

QByteArray DeviceBridge::request_tss_ticket(plist_t build_identity, plist_t identifiers, const QByteArray& nonce, uint64_t ecid)
{
    // Get Digest values from BuildManifest (used by TSS to verify image integrity)
    plist_t p_tc_digest = plist_access_path(build_identity, 3, "Manifest", "LoadableTrustCache", "Digest");
    plist_t p_dmg_digest = plist_access_path(build_identity, 3, "Manifest", "PersonalizedDMG", "Digest");
    if (!p_tc_digest || !p_dmg_digest) {
        emit MounterStatusChanged("Error: TSS: could not find digest values in BuildManifest.");
        return {};
    }

    char* tc_digest_data = nullptr;
    uint64_t tc_digest_len = 0;
    plist_get_data_val(p_tc_digest, &tc_digest_data, &tc_digest_len);

    char* dmg_digest_data = nullptr;
    uint64_t dmg_digest_len = 0;
    plist_get_data_val(p_dmg_digest, &dmg_digest_data, &dmg_digest_len);

    if (!tc_digest_data || !dmg_digest_data) {
        emit MounterStatusChanged("Error: TSS: could not read digest values from BuildManifest.");
        if (tc_digest_data) plist_mem_free(tc_digest_data);
        if (dmg_digest_data) plist_mem_free(dmg_digest_data);
        return {};
    }

    uint64_t board_id = plist_dict_get_uint(identifiers, "BoardId");
    uint64_t chip_id  = plist_dict_get_uint(identifiers, "ChipID");
    uint64_t sec_dom  = plist_dict_get_uint(identifiers, "SecurityDomain");

    // Build TSS request plist (matches go-ios tss.go getSignature params)
    plist_t req = plist_new_dict();
    plist_dict_set_item(req, "@ApImg4Ticket",     plist_new_bool(1));
    plist_dict_set_item(req, "@BBTicket",          plist_new_bool(1));
    plist_dict_set_item(req, "@HostPlatformInfo",  plist_new_string("mac"));
    plist_dict_set_item(req, "@VersionInfo",       plist_new_string("libauthinstall-973.40.2"));
    plist_dict_set_item(req, "ApBoardID",          plist_new_uint(board_id));
    plist_dict_set_item(req, "ApChipID",           plist_new_uint(chip_id));
    plist_dict_set_item(req, "ApECID",             plist_new_uint(ecid));
    plist_dict_set_item(req, "ApNonce",            plist_new_data(nonce.constData(), (uint64_t)nonce.size()));
    plist_dict_set_item(req, "ApProductionMode",   plist_new_bool(1));
    plist_dict_set_item(req, "ApSecurityDomain",   plist_new_uint(sec_dom));
    plist_dict_set_item(req, "ApSecurityMode",     plist_new_bool(1));

    plist_t tc_dict = plist_new_dict();
    plist_dict_set_item(tc_dict, "Digest",  plist_new_data(tc_digest_data, tc_digest_len));
    plist_dict_set_item(tc_dict, "EPRO",    plist_new_bool(1));
    plist_dict_set_item(tc_dict, "ESEC",    plist_new_bool(1));
    plist_dict_set_item(tc_dict, "Trusted", plist_new_bool(1));
    plist_dict_set_item(req, "LoadableTrustCache", tc_dict);

    plist_t dmg_dict = plist_new_dict();
    plist_dict_set_item(dmg_dict, "Digest",  plist_new_data(dmg_digest_data, dmg_digest_len));
    plist_dict_set_item(dmg_dict, "EPRO",    plist_new_bool(1));
    plist_dict_set_item(dmg_dict, "ESEC",    plist_new_bool(1));
    plist_dict_set_item(dmg_dict, "Name",    plist_new_string("DeveloperDiskImage"));
    plist_dict_set_item(dmg_dict, "Trusted", plist_new_bool(1));
    plist_dict_set_item(req, "PersonalizedDMG", dmg_dict);

    char sep_nonce[20] = {};
    plist_dict_set_item(req, "SepNonce", plist_new_data(sep_nonce, 20));
    plist_dict_set_item(req, "UID_MODE", plist_new_bool(0));

    plist_mem_free(tc_digest_data);
    plist_mem_free(dmg_digest_data);

    // Add all "Ap,*" keys from device identifiers (e.g. Ap,OSLongVersion, Ap,SikaFuse, etc.)
    plist_dict_iter id_iter = nullptr;
    plist_dict_new_iter(identifiers, &id_iter);
    char* id_key = nullptr;
    plist_t id_val = nullptr;
    do {
        plist_dict_next_item(identifiers, id_iter, &id_key, &id_val);
        if (id_key) {
            if (strncmp(id_key, "Ap,", 3) == 0)
                plist_dict_set_item(req, id_key, plist_copy(id_val));
            free(id_key);
            id_key = nullptr;
        }
    } while (id_val);
    plist_mem_free(id_iter);

    // Serialize to XML plist
    char* xml_data = nullptr;
    uint32_t xml_len = 0;
    plist_to_xml(req, &xml_data, &xml_len);
    plist_free(req);
    if (!xml_data) {
        emit MounterStatusChanged("Error: TSS: failed to serialize request plist.");
        return {};
    }
    QByteArray body(xml_data, (int)xml_len);
    plist_mem_free(xml_data);

    // POST to Apple TSS server
    QString netError;
    QByteArray responseData = SimpleRequest::PostSync(
        "https://gs.apple.com/TSS/controller?action=2", body, "text/xml", &netError);
    if (responseData.isEmpty()) {
        emit MounterStatusChanged("Error: TSS request failed: " + netError);
        return {};
    }

    // Response is URL-encoded: STATUS=0&MESSAGE=SUCCESS&REQUEST_STRING=<xml plist>
    // REQUEST_STRING is always last with no trailing &
    QString resp = QString::fromUtf8(responseData);

    int statusVal = -1;
    int statusIdx = resp.indexOf("STATUS=");
    if (statusIdx >= 0) {
        QString s = resp.mid(statusIdx + 7);
        int e = s.indexOf('&'); if (e >= 0) s = s.left(e);
        statusVal = s.toInt();
    }

    if (statusVal != 0) {
        QString msg;
        int msgIdx = resp.indexOf("MESSAGE=");
        if (msgIdx >= 0) { msg = resp.mid(msgIdx + 8); int e = msg.indexOf('&'); if (e >= 0) msg = msg.left(e); }
        emit MounterStatusChanged("Error: TSS server returned status " + QString::number(statusVal) + ": " + msg);
        return {};
    }

    int rsIdx = resp.indexOf("REQUEST_STRING=");
    if (rsIdx < 0) {
        emit MounterStatusChanged("Error: TSS: REQUEST_STRING missing in response.");
        return {};
    }
    QString requestString = resp.mid(rsIdx + 15); // len("REQUEST_STRING=") == 15
    int ampIdx = requestString.indexOf('&');
    if (ampIdx >= 0) requestString = requestString.left(ampIdx);

    // Parse plist and extract ApImg4Ticket
    QByteArray rsBytes = requestString.toUtf8();
    plist_t ticket_plist = nullptr;
    plist_from_xml(rsBytes.constData(), (uint32_t)rsBytes.size(), &ticket_plist);
    if (!ticket_plist) {
        emit MounterStatusChanged("Error: TSS: failed to parse response plist.");
        return {};
    }

    plist_t p_ticket = plist_dict_get_item(ticket_plist, "ApImg4Ticket");
    if (!p_ticket) {
        emit MounterStatusChanged("Error: TSS: ApImg4Ticket not found in response.");
        plist_free(ticket_plist);
        return {};
    }

    char* ticket_data = nullptr;
    uint64_t ticket_size = 0;
    plist_get_data_val(p_ticket, &ticket_data, &ticket_size);
    if (!ticket_data) {
        emit MounterStatusChanged("Error: TSS: failed to read ApImg4Ticket.");
        plist_free(ticket_plist);
        return {};
    }

    QByteArray ticket(ticket_data, (int)ticket_size);
    plist_mem_free(ticket_data);
    plist_free(ticket_plist);

    emit MounterStatusChanged("TSS signature obtained successfully.");
    return ticket;
}
