#include "extended_zip.h"
#include <zip.h>
#include <QDir>
#include <QDirIterator>

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len)
{
    struct zip_stat zs;
    struct zip_file *zfile;
    int zindex = zip_name_locate(zf, filename, locate_flags);

    *buffer = NULL;
    *len = 0;

    if (zindex < 0) {
        return -1;
    }

    zip_stat_init(&zs);

    if (zip_stat_index(zf, zindex, 0, &zs) != 0) {
        fprintf(stderr, "ERROR: zip_stat_index '%s' failed!\n", filename);
        return -2;
    }

    if (zs.size > 10485760) {
        fprintf(stderr, "ERROR: file '%s' is too large!\n", filename);
        return -3;
    }

    zfile = zip_fopen_index(zf, zindex, 0);
    if (!zfile) {
        fprintf(stderr, "ERROR: zip_fopen '%s' failed!\n", filename);
        return -4;
    }

    *buffer = (char*)malloc(zs.size);
    if (zs.size > LLONG_MAX || zip_fread(zfile, *buffer, zs.size) != (zip_int64_t)zs.size) {
        fprintf(stderr, "ERROR: zip_fread %llu bytes from '%s'\n", (uint64_t)zs.size, filename);
        free(*buffer);
        *buffer = NULL;
        zip_fclose(zfile);
        return -5;
    }
    *len = zs.size;
    zip_fclose(zfile);
    return 0;
}

int zip_get_app_directory(struct zip* zf, QString &path)
{
    int i = 0;
    int c = zip_get_num_files(zf);
    int len = 0;
    const char* name = NULL;

    /* look through all filenames in the archive */
    do {
        /* get filename at current index */
        name = zip_get_name(zf, i++, 0);
        if (name != NULL) {
            /* check if we have a "Payload/.../" name */
            len = strlen(name);
            if (!strncmp(name, "Payload/", 8) && (len > 8)) {
                /* skip hidden files */
                if (name[8] == '.')
                    continue;

                /* locate the second directory delimiter */
                const char* p = name + 8;
                do {
                    if (*p == '/') {
                        break;
                    }
                } while(p++ != NULL);

                /* try next entry if not found */
                if (p == NULL)
                    continue;

                /* copy filename */
                QFileInfo file_info(name);
                path = file_info.filePath().remove(file_info.fileName());
                break;
            }
        }
    } while(i < c);

    return 0;
}

bool zip_extract_all(QString input_zip, QString output_dir, std::function<void(int,int,QString)> callback)
{
    QDir().mkpath(output_dir);
    int err;
    char buf[100];
    zip *za = zip_open(input_zip.toUtf8().data(), 0, &err);
    if (za == nullptr) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        callback(0, 0, QString::asprintf("Can't open zip archive `%s': %s", input_zip.toUtf8().data(), buf));
        return false;
    }

    struct FileItem {
        QString filepath;
        quint64 size;
        bool isdir;
    };

    QMap<int, FileItem> list_items;
    for (quint64 idx = 0; idx < zip_get_num_entries(za, 0); idx++) {
        struct zip_stat sb;
        if (zip_stat_index(za, idx, 0, &sb) == 0) {
            size_t len = strlen(sb.name);
            FileItem item;
            item.filepath = QString(sb.name);
            item.size = sb.size;
            item.isdir = (sb.name[len - 1] == '/') ? true : false;
            list_items[idx] = item;
        }
    }

    for (int idx = 0; idx < list_items.count(); idx++) {
        FileItem item = list_items[idx];
        QString target = output_dir + "/" + QString(item.filepath);
        if (item.isdir) {
            callback(idx + 1, list_items.count(), QString("Creating `%1'...").arg(target));
            QDir().mkpath(target);
        }
        else
        {
            callback(idx + 1, list_items.count(), QString("Extracting `%1'...").arg(target));
            zip_file *zf = zip_fopen_index(za, idx, 0);
            if (!zf) {
                callback(idx + 1, list_items.count(), QString("Failed to load `%1' from zip!").arg(item.filepath));
                return false;
            }

            QFile file(target);
            if (!file.open(QIODevice::WriteOnly)) {
                callback(idx + 1, list_items.count(), "Error creating decrypted file!");
                return false;
            }
            else
            {
                char buff[1024];
                zip_uint64_t sum = 0;
                while (sum != item.size) {
                    zip_int64_t len = zip_fread(zf, static_cast<void*>(buff), 1024);
                    if (len > 0) {
                        file.write(buff, len);
                        sum += static_cast<zip_uint64_t>(len);
                    }
                    else
                    {
                        callback(idx + 1, list_items.count(), "Error creating decrypted file (2)!");
                        break;
                    }
                }
                file.close();
            }
            zip_fclose(zf);
        }
    }
    zip_close(za);
    return true;
}

std::function<void(int,int,QString)> zip_directory_callback = NULL;
void zip_progress_callback(int current, int total, const char* filename)
{
    if (zip_directory_callback)
        zip_directory_callback(current, total, QString::asprintf("Packing '%s' to package file...", filename));
}

bool zip_directory(QString input_dir, QString output_filename, std::function<void(int,int,QString)> callback)
{
    QString output_dir = QFileInfo(output_filename).absolutePath();
    QDir().mkpath(output_dir);
    int errorp;
    char buf[100];
    zip *zipper = zip_open(output_filename.toUtf8().data(), ZIP_CREATE | ZIP_EXCL, &errorp);
    if (zipper == nullptr) {
        zip_error_to_str(buf, sizeof(buf), errorp, errno);
        callback(0, 0, QString::asprintf("Can't create zip archive `%s': %s", output_filename.toUtf8().data(), buf));
        return false;
    }

    QStringList list_dirs, list_files;
    QDir dir(input_dir);
    QDirIterator it_dir(input_dir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_dir.hasNext())
        list_dirs << it_dir.next();

    QDirIterator it_file(input_dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_file.hasNext())
        list_files << it_file.next();

    int idx = 0;
    int total = list_dirs.count() + list_files.count();
    foreach (QString dir_name, list_dirs)
    {
        idx++;
        QString relativepath = dir.relativeFilePath(dir_name);
        callback(idx, total, QString::asprintf("Adding `%s' to archive...", relativepath.toUtf8().data()));
        if (zip_add_dir(zipper, relativepath.toUtf8().data()) < 0)
        {
            callback(idx, total, QString::asprintf("Can't add `%s' to archive : %s", relativepath.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
    }
    foreach (QString file_name, list_files)
    {
        idx++;
        QString relativepath = dir.relativeFilePath(file_name);
        //callback(idx, total, QString::asprintf("Adding `%s' to archive...", relativepath.toUtf8().data()));

        zip_source *source = zip_source_file(zipper, file_name.toUtf8().data(), 0, 0);
        if (source == nullptr)
        {
            zip_close(zipper);
            callback(idx, total, QString::asprintf("Can't load `%s' : %s", file_name.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
        if (zip_add(zipper, relativepath.toUtf8().data(), source) < 0)
        {
            zip_source_free(source);
            zip_close(zipper);
            callback(idx, total, QString::asprintf("Can't add `%s' to archive : %s", relativepath.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
    }
    zip_directory_callback = callback;
    int result = zip_close_with_callback(zipper, zip_progress_callback);
    zip_directory_callback = NULL;
    return result == 0;
}
