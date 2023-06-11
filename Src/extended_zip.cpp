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
}

bool ZipExtractAll(QString input_zip, QString output_dir, std::function<void (int, int, QString)> callback)
{
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
                callback(progress, total, QString("(%1 of %2) Extracting %3 to directory...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
        });
        extractor.setProgressCallback([&progress, &callback, &status, &total](const uint64_t& progresssize){
            progress = progresssize;
            if (callback) {
                callback(progress, total, QString("(%1 of %2) Extracting %3 to archive...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
            return true;
        });
        extractor.extract(input_zip.toStdString(), output_dir.toStdString());
    }
    catch (const BitException& ex)
    {
        callback(progress, total, ex.what());
        return false;
    }
    return true;
}

bool ZipDirectory(QString input_dir, QString output_filename, std::function<void (quint64, quint64, QString)> callback)
{
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
                callback(progress, total, QString("(%1 of %2) Packing %3 to archive...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
            }
        });
        compressor.setProgressCallback([&progress, &callback, &status, &total](const uint64_t& progresssize){
            progress = progresssize;
            if (callback) {
                callback(progress, total, QString("(%1 of %2) Packing %3 to archive...").arg(BytesToString(progress)).arg(BytesToString(total)).arg(status));
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
        callback(progress, total, ex.what());
        return false;
    }
    return true;
}
