#include "extended_zip.h"
#include <zip.h>
#include <QDir>
#include <QDirIterator>

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

//===================================//
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
using namespace bit7z;

bool ZipGetContents(QString zip_file, QString inside_path, std::vector<char>& data_out)
{
    try
    {
#if defined(WIN32) && defined(NDEBUG)
        Bit7zLibrary lib{ "7z.dll" };
#elif defined(WIN32) && defined(DEBUG)
        Bit7zLibrary lib{ "7z_d.dll" };
#elif defined(NDEBUG)
        Bit7zLibrary lib{ "7z.so" };)
#elif defined(DEBUG)
        Bit7zLibrary lib{ "7z_d.so" };
#endif
        BitArchiveReader arc{ lib, zip_file.toStdString(), BitFormat::Zip };
        auto it = arc.find(inside_path.toStdString());
        if (it != arc.end())
        {
            std::vector<byte_t> data_temp;
            arc.extract(data_temp, it->index());
            data_out = std::vector<char>(data_temp.begin(), data_temp.end());
            return true;
        }
        qDebug() << "Not found";
    }
    catch ( const BitException& ex )
    {
        qDebug() << ex.what();
    }
    return false;
}

bool ZipGetAppDirectory(QString zip_file, QString &path_out)
{
    try
    {
#if defined(WIN32) && defined(NDEBUG)
        Bit7zLibrary lib{ "7z.dll" };
#elif defined(WIN32) && defined(DEBUG)
        Bit7zLibrary lib{ "7z_d.dll" };
#elif defined(NDEBUG)
        Bit7zLibrary lib{ "7z.so" };)
#elif defined(DEBUG)
        Bit7zLibrary lib{ "7z_d.so" };
#endif
        BitArchiveReader arc{ lib, zip_file.toStdString(), BitFormat::Zip };
        for (auto itr = arc.begin(); itr != arc.end(); ++itr)
        {
            QString path_in(itr->path().c_str());
            if (path_in.contains(".app", Qt::CaseInsensitive))
            {
                qsizetype idx = path_in.indexOf(".app",Qt::CaseInsensitive);
                path_out = path_in.mid(0, idx + 4);
                return true;
            }
        }
        qDebug() << "Not found";
    }
    catch ( const BitException& ex )
    {
        qDebug() << ex.what();
    }
    return false;
}
