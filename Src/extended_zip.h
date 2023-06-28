#pragma once
#include <QString>

bool ZipGetContents(QString zip_file, QString inside_path, std::vector<char>& data_out);
bool ZipGetAppDirectory(QString zip_file, QString& path_out);
bool ZipExtractAll(QString input_zip, QString output_dir, std::function<void(float,QString)> callback);
bool ZipDirectory(QString input_dir, QString output_filename, std::function<void(float,QString)> callback);
