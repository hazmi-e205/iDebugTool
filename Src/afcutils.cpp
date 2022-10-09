#include "devicebridge.h"

#include <libimobiledevice-glue/utils.h>
#include <dirent.h>

#ifdef WIN32
#include <windows.h>
#define S_IFLNK S_IFREG
#define S_IFSOCK S_IFREG
#endif
#include <string.h>

int DeviceBridge::afc_upload_file(afc_client_t &afc, QString &filename, QString &dstfn)
{
    FILE *f = NULL;
    uint64_t af = 0;
    char buf[1048576];

    f = fopen(filename.toUtf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        return -1;
    }

    if ((afc_file_open(afc, dstfn.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
        fclose(f);
        fprintf(stderr, "afc_file_open on '%s' failed!\n", dstfn.toUtf8().data());
        return -1;
    }

    size_t amount = 0;
    do {
        amount = fread(buf, 1, sizeof(buf), f);
        if (amount > 0) {
            uint32_t written, total = 0;
            while (total < amount) {
                written = 0;
                afc_error_t aerr = afc_file_write(afc, af, buf, amount, &written);
                if (aerr != AFC_E_SUCCESS) {
                    fprintf(stderr, "AFC Write error: %d\n", aerr);
                    break;
                }
                total += written;
            }
            if (total != amount) {
                fprintf(stderr, "Error: wrote only %u of %u\n", total, (uint32_t)amount);
                afc_file_close(afc, af);
                fclose(f);
                return -1;
            }
        }
    } while (amount > 0);

    afc_file_close(afc, af);
    fclose(f);

    return 0;
}

void DeviceBridge::afc_upload_dir(afc_client_t &afc, QString &path, QString &afcpath)
{
    afc_make_directory(afc, afcpath.toUtf8().data());

    DIR *dir = opendir(path.toUtf8().data());
    if (dir) {
        struct dirent* ep;
        while ((ep = readdir(dir))) {
            if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0)) {
                continue;
            }
            QString fpath = path + "/" + ep->d_name;
            QString apath = afcpath + "/" + ep->d_name;

            struct stat st;

            if ((stat(fpath.toUtf8().data(), &st) == 0) && S_ISDIR(st.st_mode)) {
                afc_upload_dir(afc, fpath, apath);
            } else {
                afc_upload_file(afc, fpath, apath);
            }
        }
        closedir(dir);
    }
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
    m_crashtargetdir = (target_dir == nullptr ? m_crashtargetdir : target_dir);
    const char* target_directory = (target_dir == nullptr ? m_crashtargetdir.toUtf8().data() : target_dir);

    if (!afc)
        return res;

    char** list = NULL;
        afc_error = afc_read_directory(afc, device_directory, &list);
        if (afc_error != AFC_E_SUCCESS) {
            fprintf(stderr, "ERROR: Could not read device directory '%s'\n", device_directory);
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
                printf("Failed to read information for '%s'. Skipping...\n", source_filename);
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
                    printf("Link: %s\n", (char*)target_filename + strlen(target_directory));

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
                    fprintf(stderr, "Unable to open device file '%s' (%d). Skipping...\n", source_filename, afc_error);
                    continue;
                }

                FILE* output = fopen(target_filename, "wb");
                if(output == NULL) {
                    fprintf(stderr, "Unable to open local file '%s'. Skipping...\n", target_filename);
                    afc_file_close(afc, handle);
                    continue;
                }

                printf("%s: %s\n", (true ? "Copy": "Move") , (char*)target_filename + strlen(target_directory));

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
                    fprintf(stderr, "File size mismatch. Skipping...\n");
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
