#pragma once
#include <QString>

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len);
int zip_get_app_directory(struct zip* zf, QString &path);
bool zip_extract_all(QString input_zip, QString output_dir, std::function<void(int,int,QString)> callback);
bool zip_directory(QString input_dir, QString output_filename, std::function<void(int,int,QString)> callback);
