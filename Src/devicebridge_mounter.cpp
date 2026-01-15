#include "devicebridge.h"
#include "extended_plist.h"
#include <QFileInfo>

QStringList DeviceBridge::GetMountedImages()
{
    QStringList signatures;
    if (!m_imageMounter)
        return signatures;

    QJsonDocument doc;
    plist_t result = nullptr;
    mobile_image_mounter_error_t err = mobile_image_mounter_lookup_image(m_imageMounter, "Developer", &result);
    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
    {
        doc = PlistToJson(result);
        auto arr = doc["ImageSignature"].toArray();
        for (int idx = 0; idx < arr.count(); idx++)
        {
            signatures.append(arr[idx].toString());
        }
    }
    else
    {
        emit MessagesReceived(MessagesType::MSG_ERROR, "Error: lookup_image returned " + QString::number(err));
    }

    if (result)
        plist_free(result);
    return signatures;
}

bool DeviceBridge::IsImageMounted()
{
    QStringList mounted = DeviceBridge::Get()->GetMountedImages();
    return !mounted.empty();
}

void DeviceBridge::MountImage(QString image_path, QString signature_path)
{
    AsyncManager::Get()->StartAsyncRequest([this, image_path, signature_path]() {
        unsigned char *sig = NULL;
        size_t sig_length = 0;
        size_t image_size = 0;
        mobile_image_mounter_error_t err = MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
        plist_t result = NULL;

        FILE *f = fopen(signature_path.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening signature file '" + signature_path + "' : " + strerror(errno));
            return;
        }
        sig_length = fread(sig, 1, sizeof(sig), f);
        fclose(f);
        if (sig_length == 0) {
            emit MounterStatusChanged("Error: Could not read signature from file '" + signature_path + "'");
            return;
        }

        f = fopen(image_path.toUtf8().data(), "rb");
        if (!f) {
            emit MounterStatusChanged("Error: opening image file '" + image_path + "' : " + strerror(errno));
            return;
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
        QStringList os_version = m_deviceInfo[m_currentUdid]["ProductVersion"].toString().split(".");
        if (os_version[0].toInt() >= 7) {
            mount_type = DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE;
        }

        switch (mount_type) {
        case DISK_IMAGE_UPLOAD_TYPE_UPLOAD_IMAGE:
            emit MounterStatusChanged("Uploading '" + QFileInfo(image_path).fileName() + "' to device...");
            err = mobile_image_mounter_upload_image(m_imageMounter, "Developer", image_size, sig, sig_length, ImageMounterCallback, f);
            if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
                QString message("ERROR: Unknown error occurred, can't mount.");
                if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
                    message = "ERROR: Device is locked, can't mount. Unlock device and try again.";
                }
                emit MounterStatusChanged("Error: " + message);
                return;
            }
            break;

        default:
            emit MounterStatusChanged("Uploading " + QFileInfo(image_path).fileName() + " --> afc:///" + targetname);
            char **strs = NULL;
            if (afc_get_file_info(m_afc, PKG_PATH, &strs) != AFC_E_SUCCESS) {
                if (afc_make_directory(m_afc, PKG_PATH) != AFC_E_SUCCESS) {
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
            if ((afc_file_open(m_afc, targetname.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
                fclose(f);
                emit MounterStatusChanged("Error: afc_file_open on '" + targetname + "' failed!");
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
                        if (afc_file_write(m_afc, af, buf + total, amount - total, &written) != AFC_E_SUCCESS) {
                            emit MounterStatusChanged("Error: AFC Write error!");
                            break;
                        }
                        total += written;
                    }
                    if (total != amount) {
                        emit MounterStatusChanged("Error: wrote only " + QString::number(total) + " of " + QString::number(amount));
                        afc_file_close(m_afc, af);
                        fclose(f);
                        return;
                    }
                }
            }
            while (amount > 0);

            afc_file_close(m_afc, af);
            break;
        }
        fclose(f);
        emit MounterStatusChanged("Image uploaded.");

        emit MounterStatusChanged("Mounting...");
        err = mobile_image_mounter_mount_image(m_imageMounter, mountname.toUtf8().data(), sig, sig_length, "Developer", &result);
        if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS)
        {
            emit MounterStatusChanged("Developer disk image mounted");
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
    });
}

ssize_t DeviceBridge::ImageMounterCallback(void *buf, size_t size, void *userdata)
{
    if (!m_destroyed)
        return fread(buf, 1, size, (FILE*)userdata);
}