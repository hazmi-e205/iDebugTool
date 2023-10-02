#include "extended_zip.h"
#include <zip.h>
#include <QDir>
#include <QDirIterator>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <QSaveFile>
#include "utils.h"
#include "zip/src/zip.h"
#include <fstream>
using namespace bit7z;

bool ZipGetContents(QString zip_file, QString inside_path, std::vector<char>& data_out)
{
#ifdef USE_BIT7Z
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
#else
    void *buf = NULL;
    size_t bufsize;
    struct zip_t *zip = zip_open(zip_file.toStdString().c_str(), 0, 'r');
    {
        zip_entry_open(zip, inside_path.toStdString().c_str());
        {
            zip_entry_read(zip, &buf, &bufsize);
            char *charBuf = (char*)buf;
            data_out = std::vector<char>(charBuf, charBuf + bufsize);
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
    free(buf);
    return true;
#endif
}

bool ZipGetAppDirectory(QString zip_file, QString &path_out)
{
#ifdef USE_BIT7Z
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
        for (auto it = arc.begin(); it != arc.end(); ++it)
        {
            QString path_in(it->path().c_str());
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
#else
    struct zip_t *zip = zip_open(zip_file.toStdString().c_str(), 0, 'r');
    int i, n = zip_entries_total(zip);
    for (i = 0; i < n; ++i) {
        zip_entry_openbyindex(zip, i);
        {
            QString path_in = zip_entry_name(zip);
            if (path_in.contains(".app", Qt::CaseInsensitive))
            {
                qsizetype idx = path_in.indexOf(".app",Qt::CaseInsensitive);
                path_out = path_in.mid(0, idx + 4);
                return true;
            }
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
    return false;
#endif
}

bool ZipExtractAll(QString input_zip, QString output_dir, std::function<void (float, QString)> callback)
{
#ifdef USE_BIT7Z
    QString status = "";
    quint64 progress = 0, total = 0;
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
        BitFileExtractor extractor{ lib, BitFormat::Zip };
        extractor.setTotalCallback([&total](const uint64_t& totalsize){
            total = totalsize;
        });
        extractor.setFileCallback([&progress, &callback, &status, &total](const tstring& currentfile){
            status = QString::fromStdString(currentfile);
            if (callback) {
                float percentage = ((float)progress / (float)total) * 100.f;
                callback(percentage, QString("(%1 of %2) Extracting %3 to directory...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
        });
        extractor.setProgressCallback([&progress, &callback, &status, &total](const uint64_t& progresssize){
            progress = progresssize;
            if (callback) {
                float percentage = ((float)progress / (float)total) * 100.f;
                callback(percentage, QString("(%1 of %2) Extracting %3 to directory...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
            return true;
        });
        extractor.extract(input_zip.toStdString(), output_dir.toStdString());
    }
    catch (const BitException& ex)
    {
        float percentage = ((float)progress / (float)total) * 100.f;
        callback(percentage, ex.what());
        return false;
    }
#else
    QDir().mkpath(output_dir);
    struct zip_t *zip = zip_open(input_zip.toStdString().c_str(), 0, 'r');
    int i, n = zip_entries_total(zip);
    for (i = 0; i < n; ++i) {
        zip_entry_openbyindex(zip, i);
        {
            QString target = output_dir + "/" + QString(zip_entry_name(zip));
            if (zip_entry_isdir(zip)) {
                callback(float(i + 1) * 100.f / n, QString("Creating `%1'...").arg(target));
                QDir().mkpath(target);
            } else {
                callback(float(i + 1) * 100.f / n, QString("Extracting `%1'...").arg(target));
                zip_entry_fread(zip, target.toStdString().c_str());
            }
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
#endif
    return true;
}

bool ZipDirectory(QString input_dir, QString output_filename, std::function<void (float, QString)> callback)
{
#ifdef USE_BIT7Z
    QString status = "";
    quint64 progress = 0, total = 0;
    QString basedir = GetBaseDirectory(output_filename);
    QDir().mkpath(basedir);

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
        BitFileCompressor compressor{ lib, BitFormat::Zip };
        compressor.setTotalCallback([&total](const uint64_t& totalsize){
            total = totalsize;
        });
        compressor.setFileCallback([&progress, &callback, &status, &total](const tstring& currentfile){
            status = QString::fromStdString(currentfile);
            if (callback) {
                float percentage = ((float)progress / (float)total) * 100.f;
                callback(percentage, QString("(%1 of %2) Packing %3 to archive...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
        });
        compressor.setProgressCallback([&progress, &callback, &status, &total](const uint64_t& progresssize){
            progress = progresssize;
            if (callback) {
                float percentage = ((float)progress / (float)total) * 100.f;
                callback(percentage, QString("(%1 of %2) Packing %3 to archive...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
            return true;
        });

        std::map<std::string, std::string> files;
        QDir dir(input_dir);
        QDirIterator it_file(input_dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it_file.hasNext())
        {
            QString filepath = it_file.next();
            QString relativepath = dir.relativeFilePath(filepath);
            files[filepath.toStdString()] = relativepath.toStdString();
        }
        compressor.compress(files, output_filename.toStdString());
    }
    catch ( const BitException& ex )
    {
        float percentage = ((float)progress / (float)total) * 100.f;
        callback(percentage, ex.what());
        return false;
    }
#else
    std::unordered_map<std::string, std::string> files;
    QDir dir(input_dir);
    QDirIterator it_file(input_dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_file.hasNext())
    {
        QString filepath = it_file.next();
        QString relativepath = dir.relativeFilePath(filepath);
        files[filepath.toStdString()] = relativepath.toStdString();
    }

    QDir().mkpath(GetBaseDirectory(output_filename));
    struct zip_t *zip = zip_open(output_filename.toStdString().c_str(), 0, 'w');
    {
        size_t count = 0;
        for (auto const& x : files)
        {
            count += 1;
            float percentage = ((float) count/ (float)files.size()) * 100.f;
            callback(percentage, QString("(%1 of %2) Packing %3 to archive...").arg(count).arg(files.size()).arg(x.second.c_str()));

            int aaa = zip_entry_open(zip, x.second.c_str());
            std::ifstream ifile;
            ifile.open(x.first, std::ios::binary | std::ios::in | std::ios::ate);
            if(ifile.good()){
                char *buffer;
                long size;
                std::ifstream file(x.first, std::ios::in | std::ios::binary | std::ios::ate);
                size = file.tellg();
                file.seekg(0, std::ios::beg);
                buffer = new char[size];
                file.read(buffer, size);
                aaa = zip_entry_write(zip, buffer, size);
            }
            aaa = zip_entry_close(zip);
        }
    }
    zip_close(zip);
#endif
    return true;
}
