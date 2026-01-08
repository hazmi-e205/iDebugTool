#include "devicebridge.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <libimobiledevice-glue/utils.h>
#include <dirent.h>

#ifdef WIN32
#include <windows.h>
#define S_IFLNK S_IFREG
#define S_IFSOCK S_IFREG
#else
#include <unistd.h>
#endif
#include <string.h>
#include "utils.h"


void DeviceBridge::SyncCrashlogs(QString path)
{
    AsyncManager::Get()->StartAsyncRequest([this, path]() {
        QDir().mkpath(path);
        int result = afc_copy_crash_reports(m_crashlog, ".", path.toUtf8().data(), path.toUtf8().data());
        emit CrashlogsStatusChanged(QString::asprintf("Done, error code: %d", result));
    });
}

void DeviceBridge::GetAccessibleStorage(QString startPath, QString bundleId)
{
    AsyncManager::Get()->StartAsyncRequest([this, startPath, bundleId]() {
        m_accessibleStorage.clear();

        if (!bundleId.isEmpty()) {
            QStringList serviceIds = QStringList() << HOUSE_ARREST_SERVICE_NAME;
            StartLockdown(!m_houseArrest, serviceIds, [this, bundleId](QString& service_id, lockdownd_service_descriptor_t& service){
                house_arrest_error_t err = house_arrest_client_new(m_device, service, &m_houseArrest);
                if (err != HOUSE_ARREST_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));

                err = house_arrest_send_command(m_houseArrest, "VendContainer", bundleId.toUtf8().data());
                if (err != HOUSE_ARREST_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Access Denied to " + bundleId + "'s VendContainer client! " + QString::number(err));

                afc_error_t aerr = afc_client_new_from_house_arrest_client(m_houseArrest, &m_fileManager);
                if (aerr != AFC_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to afc with " + service_id + " client! " + QString::number(aerr));
            });
        } else {
            QStringList serviceIds = QStringList() << AFC_SERVICE_NAME;
            StartLockdown(!m_fileManager, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
                afc_error_t aerr = afc_client_new(m_device, service, &m_fileManager);
                if (aerr != AFC_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(aerr));
            });
        }

        afc_traverse_recursive(m_fileManager, startPath.toStdString().c_str());

        emit AccessibleStorageReceived(m_accessibleStorage);
    });
}

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

int file_exists(const char* path)
{
    struct stat tst;
#ifdef WIN32
    return (stat(path, &tst) == 0);
#else
    return (lstat(path, &tst) == 0);
#endif
}

int DeviceBridge::afc_copy_crash_reports(afc_client_t &afc, const char* device_directory, const char* host_directory, const char* target_dir, const char* filename_filter)
{
    afc_error_t afc_error;
    int k;
    int res = -1;
    int crash_report_count = 0;
    uint64_t handle;
    char source_filename[512];
    char target_filename[512];
    m_crashlogTargetDir = (target_dir == nullptr ? m_crashlogTargetDir : target_dir);
    const char* target_directory = (target_dir == nullptr ? m_crashlogTargetDir.toUtf8().data() : target_dir);

    if (!afc)
        return res;

    char** list = NULL;
        afc_error = afc_read_directory(afc, device_directory, &list);
        if (afc_error != AFC_E_SUCCESS) {
            emit CrashlogsStatusChanged(QString("ERROR: Could not read device directory '%1'").arg(device_directory));
            return res;
        }

        /* ensure we have a trailing slash */
        strcpy(source_filename, device_directory);
        if (source_filename[strlen(source_filename)-1] != '/') {
            strcat(source_filename, "/");
        }
        int device_directory_length = strlen(source_filename);

        /* ensure we have a trailing slash */
        strcpy(target_filename, host_directory);
        if (target_filename[strlen(target_filename)-1] != '/') {
            strcat(target_filename, "/");
        }
        int host_directory_length = strlen(target_filename);

        /* loop over file entries */
        for (k = 0; list[k]; k++) {
            if (!strcmp(list[k], ".") || !strcmp(list[k], "..")) {
                continue;
            }

            char **fileinfo = NULL;
            struct stat stbuf;
            memset(&stbuf, '\0', sizeof(struct stat));

            /* assemble absolute source filename */
            strcpy(((char*)source_filename) + device_directory_length, list[k]);

            /* assemble absolute target filename */
    #ifdef WIN32
            /* replace every ':' with '-' since ':' is an illegal character for file names in windows */
            char* current_pos = strchr(list[k], ':');
            while (current_pos) {
                *current_pos = '-';
                current_pos = strchr(current_pos, ':');
            }
    #endif
            char* p = strrchr(list[k], '.');
            if (p != NULL && !strncmp(p, ".synced", 7)) {
                /* make sure to strip ".synced" extension as seen on iOS 5 */
                int newlen = strlen(list[k]) - 7;
                strncpy(((char*)target_filename) + host_directory_length, list[k], newlen);
                target_filename[host_directory_length + newlen] = '\0';
            } else {
                strcpy(((char*)target_filename) + host_directory_length, list[k]);
            }

            /* get file information */
            afc_get_file_info(afc, source_filename, &fileinfo);
            if (!fileinfo) {
                emit CrashlogsStatusChanged(QString("Failed to read information for '%1'. Skipping...").arg(source_filename));
                continue;
            }

            /* parse file information */
            int i;
            for (i = 0; fileinfo[i]; i+=2) {
                if (!strcmp(fileinfo[i], "st_size")) {
                    stbuf.st_size = atoll(fileinfo[i+1]);
                } else if (!strcmp(fileinfo[i], "st_ifmt")) {
                    if (!strcmp(fileinfo[i+1], "S_IFREG")) {
                        stbuf.st_mode = S_IFREG;
                    } else if (!strcmp(fileinfo[i+1], "S_IFDIR")) {
                        stbuf.st_mode = S_IFDIR;
                    } else if (!strcmp(fileinfo[i+1], "S_IFLNK")) {
                        stbuf.st_mode = S_IFLNK;
                    } else if (!strcmp(fileinfo[i+1], "S_IFBLK")) {
                        stbuf.st_mode = S_IFBLK;
                    } else if (!strcmp(fileinfo[i+1], "S_IFCHR")) {
                        stbuf.st_mode = S_IFCHR;
                    } else if (!strcmp(fileinfo[i+1], "S_IFIFO")) {
                        stbuf.st_mode = S_IFIFO;
                    } else if (!strcmp(fileinfo[i+1], "S_IFSOCK")) {
                        stbuf.st_mode = S_IFSOCK;
                    }
                } else if (!strcmp(fileinfo[i], "st_nlink")) {
                    stbuf.st_nlink = atoi(fileinfo[i+1]);
                } else if (!strcmp(fileinfo[i], "st_mtime")) {
                    stbuf.st_mtime = (time_t)(atoll(fileinfo[i+1]) / 1000000000);
                } else if (!strcmp(fileinfo[i], "LinkTarget")) {
                    /* report latest crash report filename */
                    emit CrashlogsStatusChanged(QString("Link: %1").arg(target_filename));

                    /* remove any previous symlink */
                    if (file_exists(target_filename)) {
                        remove(target_filename);
                    }

    #ifndef WIN32
                    /* use relative filename */
                    char* b = strrchr(fileinfo[i+1], '/');
                    if (b == NULL) {
                        b = fileinfo[i+1];
                    } else {
                        b++;
                    }

                    /* create a symlink pointing to latest log */
                    if (symlink(b, target_filename) < 0) {
                        fprintf(stderr, "Can't create symlink to %s\n", b);
                    }
    #endif
                    res = 0;
                }
            }

            /* free file information */
            afc_dictionary_free(fileinfo);

            /* recurse into child directories */
            if (S_ISDIR(stbuf.st_mode)) {
    #ifdef WIN32
                mkdir(target_filename);
    #else
                mkdir(target_filename, 0755);
    #endif
                res = afc_copy_crash_reports(afc, source_filename, target_filename, nullptr, filename_filter);
            }
            else if (S_ISREG(stbuf.st_mode))
            {
                if (filename_filter != NULL && strstr(source_filename, filename_filter) == NULL) {
                    continue;
                }

                /* copy file to host */
                afc_error = afc_file_open(afc, source_filename, AFC_FOPEN_RDONLY, &handle);
                if(afc_error != AFC_E_SUCCESS) {
                    if (afc_error == AFC_E_OBJECT_NOT_FOUND) {
                        continue;
                    }
                    emit CrashlogsStatusChanged(QString("Unable to open device file '%1' (%2). Skipping...").arg(source_filename, afc_error));
                    continue;
                }

                FILE* output = fopen(target_filename, "wb");
                if(output == NULL) {
                    emit CrashlogsStatusChanged(QString("Unable to open local file '%1'. Skipping...").arg(target_filename));
                    afc_file_close(afc, handle);
                    continue;
                }

                emit CrashlogsStatusChanged(QString("Copy: %1...").arg(target_filename));

                uint32_t bytes_read = 0;
                uint32_t bytes_total = 0;
                unsigned char data[0x1000];

                afc_error = afc_file_read(afc, handle, (char*)data, 0x1000, &bytes_read);
                while(afc_error == AFC_E_SUCCESS && bytes_read > 0) {
                    fwrite(data, 1, bytes_read, output);
                    bytes_total += bytes_read;
                    afc_error = afc_file_read(afc, handle, (char*)data, 0x1000, &bytes_read);
                }
                afc_file_close(afc, handle);
                fclose(output);

                if ((uint32_t)stbuf.st_size != bytes_total) {
                    emit CrashlogsStatusChanged("File size mismatch. Skipping...");
                    continue;
                }

                crash_report_count++;
                res = 0;
            }
        }
        afc_dictionary_free(list);

        /* no reports, no error */
        if (crash_report_count == 0)
            res = 0;

        return res;
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

            m_accessibleStorage[QString(full_path)] = FileProperty(is_dir, size_bytes);
            qDebug() << QString(full_path) << " | " << is_dir << " | " << size_bytes;

            afc_dictionary_free(info);
        }

        if (is_dir) {
            afc_traverse_recursive(afc, full_path);
        }
    }
    afc_dictionary_free(file_list);
}
