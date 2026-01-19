#include "devicebridge.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <libimobiledevice-glue/utils.h>
#include <dirent.h>

#include <string.h>
#include "utils.h"


int DeviceBridge::afc_upload_file(afc_client_t &afc, const QString &filename, const QString &dstfn, std::function<void(uint32_t,uint32_t)> callback)
{
    FILE *f = NULL;
    uint64_t af = 0;
    char buf[1048576];
    size_t total_bytes = QFileInfo(filename).size();
    size_t uploaded_bytes = 0;

    f = fopen(filename.toUtf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        return -2;
    }

    if ((afc_file_open(afc, dstfn.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
        fclose(f);
        fprintf(stderr, "afc_file_open on '%s' failed!\n", dstfn.toUtf8().data());
        return -2;
    }

    size_t amount = 0;
    do {
        amount = fread(buf, 1, sizeof(buf), f);
        if (amount > 0) {
            uint32_t written, total = 0;
            afc_error_t aerr = AFC_E_SUCCESS;
            while (total < amount) {
                written = 0;
                aerr = afc_file_write(afc, af, buf, amount, &written);
                if (aerr != AFC_E_SUCCESS) {
                    break;
                }
                total += written;
                uploaded_bytes += written;
                if (callback) callback(uploaded_bytes, total_bytes);
            }
            if (total != amount) {
                fprintf(stderr, "Error: wrote only %u of %u\n", total, (uint32_t)amount);
                afc_file_close(afc, af);
                fclose(f);
                return aerr;
            }
        }
    } while (amount > 0);

    afc_file_close(afc, af);
    fclose(f);
    return AFC_E_SUCCESS;
}

bool DeviceBridge::afc_upload_dir(afc_client_t &afc, const QString &path, const QString &afcpath, std::function<void(int,int,QString)> callback)
{
    QStringList list_dirs, list_files;
    QDir dirpath(path);
    QDirIterator it_dir(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    list_dirs << path;
    while (it_dir.hasNext())
        list_dirs << it_dir.next();

    QDirIterator it_file(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_file.hasNext())
        list_files << it_file.next();

    int idx = 0;
    int total = list_dirs.count() + list_files.count();
    foreach (QString dir_name, list_dirs)
    {
        idx++;
        QString targetpath = afcpath + "/" + dirpath.relativeFilePath(dir_name);
        if (callback) callback(idx, total, QString::asprintf("Adding `%s' to device...", targetpath.toUtf8().data()));
        afc_error_t result = afc_make_directory(afc, targetpath.toUtf8().data());
        if (result != AFC_E_SUCCESS)
        {
            if (callback) callback(idx, total, QString::asprintf("Can't add `%s' to device : afc error code %d", targetpath.toUtf8().data(), result));
            return false;
        }
    }
    foreach (QString file_name, list_files)
    {
        idx++;
        QString targetpath = afcpath + "/" + dirpath.relativeFilePath(file_name);
        auto afc_callback = [&](uint32_t uploaded_bytes, uint32_t total_bytes)
        {
            if (callback) callback(idx, total, QString::asprintf("(%s of %s) Sending `%s' to device...",
                                                                 BytesToString(uploaded_bytes).toUtf8().data(),
                                                                 BytesToString(total_bytes).toUtf8().data(),
                                                                 targetpath.toUtf8().data()));
        };
        int result = afc_upload_file(afc, file_name, targetpath, afc_callback);
        if (result != 0)
        {
            if (callback) callback(idx, total, QString::asprintf("Can't send `%s' to device : afc error code %d", targetpath.toUtf8().data(), result));
            return false;
        }
    }
    return true;
}

void DeviceBridge::afc_traverse_recursive(afc_client_t afc, const char *path)
{
    char **file_list = NULL;

    if (afc_read_directory(afc, path, &file_list) != AFC_E_SUCCESS || !file_list) {
        return;
    }

    for (int i = 0; file_list[i]; i++) {
        if (strcmp(file_list[i], ".") == 0 || strcmp(file_list[i], "..") == 0) continue;

        char full_path[2048];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", file_list[i]);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, file_list[i]);
        }

        char **info = NULL;
        bool is_dir = false;
        quint64 size_bytes = 0;

        if (afc_get_file_info(afc, full_path, &info) == AFC_E_SUCCESS && info) {

            for (int j = 0; info[j]; j += 2) {
                if (std::strcmp(info[j], "st_ifmt") == 0 && std::strcmp(info[j + 1], "S_IFDIR") == 0) {
                    is_dir = true;
                }
                if (std::strcmp(info[j], "st_size") == 0) {
                    // Convert string to uint64_t
                    try {
                        size_bytes = std::stoull(info[j + 1]);
                    } catch (...) {
                        size_bytes = 0;
                    }
                }
            }

            m_accessibleStorage[QString(full_path)] = FileProperty{is_dir, size_bytes};
            // qDebug() << QString(full_path) << " | " << is_dir << " | " << size_bytes;
            afc_dictionary_free(info);
        }

        if (is_dir) {
            afc_traverse_recursive(afc, full_path);
        }
    }
    afc_dictionary_free(file_list);
}
