#pragma once
#include <QString>

bool ZipGetContents(QString zip_file, QString inside_path, std::vector<char>& data_out);
bool ZipGetAppDirectory(QString zip_file, QString& path_out);
bool ZipExtractAll(QString input_zip, QString output_dir, std::function<void(int,int,QString)> callback);
bool ZipDirectory(QString input_dir, QString output_filename, std::function<void(quint64,quint64,QString)> callback);

bool zip_extract_all(QString input_zip, QString output_dir, std::function<void(int,int,QString)> callback);
bool zip_directory(QString input_dir, QString output_filename, std::function<void(int,int,QString)> callback);
