#include "devicebridge.h"
#include "extended_plist.h"
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
    StartLockdown(!m_imageMounter, m_mounterClient, serviceIds, [&, this](QString& service_id, lockdownd_service_descriptor_t& service){
        mobile_image_mounter_error_t err = mobile_image_mounter_new(m_device, service, &m_imageMounter);
        if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
            return;
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
            err = mobile_image_mounter_lookup_image(m_imageMounter, type.toUtf8().data(), &result);
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
        if (m_imageMounter)
        {
            mobile_image_mounter_hangup(m_imageMounter);
            mobile_image_mounter_free(m_imageMounter);
            m_imageMounter = nullptr;
        }
    });
    return signatures;
}

bool DeviceBridge::IsImageMounted()
{
    QStringList mounted = DeviceBridge::Get()->GetMountedImages();
    return !mounted.empty();
}

void DeviceBridge::MountImage(QString image_path, QString signature_path)
{
    QStringList serviceIds = QStringList() << MOBILE_IMAGE_MOUNTER_SERVICE_NAME;
    StartLockdown(!m_imageMounter, m_mounterClient, serviceIds, [&, this](QString& service_id, lockdownd_service_descriptor_t& service){
        mobile_image_mounter_error_t err = mobile_image_mounter_new(m_device, service, &m_imageMounter);
        if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));
        }
    }, false);

    serviceIds = QStringList() << AFC_SERVICE_NAME;
    StartLockdown(!m_imageSender, m_mounterClient, serviceIds, [&, this](QString& service_id, lockdownd_service_descriptor_t& service){
        afc_error_t aerr = afc_client_new(m_device, service, &m_imageSender);
        if (aerr != AFC_E_SUCCESS) {
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(aerr));
        }
    }, false);

    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        mount_image(m_imageSender, image_path, signature_path);
        if (m_imageSender) {
            afc_client_free(m_imageSender);
            m_imageSender = nullptr;
        }
        if (m_imageMounter)
        {
            mobile_image_mounter_hangup(m_imageMounter);
            mobile_image_mounter_free(m_imageMounter);
            m_imageMounter = nullptr;
        }
        if (!m_mounterClient) {
            lockdownd_client_free(m_mounterClient);
            m_mounterClient = nullptr;
        }
    });
}

void DeviceBridge::mount_image(afc_client_t &afc, QString image_path, QString signature_path)
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

        mobile_image_mounter_error_t merr = mobile_image_mounter_query_personalization_identifiers(m_imageMounter, NULL, &identifiers);
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

        unsigned char buf[8192];
        unsigned char sha384_digest[48];
        sha384_context ctx;
        sha384_init(&ctx);
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
        while (!feof(f)) {
            size_t fr = fread(buf, 1, sizeof(buf), f);
            if (fr == 0) {
                break;
            }
            sha384_update(&ctx, buf, fr);
        }
        rewind(f);
        sha384_final(&ctx, sha384_digest);

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
            merr = mobile_image_mounter_query_personalization_manifest(
                m_imageMounter, "DeveloperDiskImage", sha384_digest, sizeof(sha384_digest), &manifest, &manifest_size);
            if (merr != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
                emit MounterStatusChanged("Error: No personalization manifest available. Provide an IM4M signature file.");
                fclose(f);
                plist_free(build_manifest);
                plist_free(identifiers);
                plist_free(mount_options);
                return;
            }
            sig = manifest;
            sig_length = manifest_size;
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
        err = mobile_image_mounter_upload_image(m_imageMounter, image_type, image_size, sig, sig_length, ImageMounterCallback, f);
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
            m_imageMounter, mountname.toUtf8().data(), sig, sig_length, image_type, mount_options, &result);
    } else {
        err = mobile_image_mounter_mount_image(
            m_imageMounter, mountname.toUtf8().data(), sig, sig_length, image_type, &result);
    }
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
    {
        emit MounterStatusChanged(is_personalized ? "Personalized disk image mounted" : "Developer disk image mounted");
        ConnectToDevice(m_currentUdid); //hack to fix LOCKDOWN_E_MUX_ERROR after mounted
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
