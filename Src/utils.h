#ifndef UTILS_H
#define UTILS_H

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <plist/plist.h>
#include <libimobiledevice/afc.h>
#include "logpacket.h"

QJsonObject PlistToJsonObject(plist_t node);
QJsonDocument PlistToJson(plist_t node);
QJsonArray PlistToJsonArray(plist_t node);
QJsonValue PlistDictToJsonValue(plist_t node);
QJsonValue PlistNodeToJsonValue(plist_t node);
QJsonValue PlistArrayToJsonValue(plist_t node);
QString Base64Encode(QString string);
bool ParseSystemLogs(char &in, LogPacket &out);

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len);
int zip_get_app_directory(struct zip* zf, QString &path);
int afc_upload_file(afc_client_t afc, QString &filename, QString &dstfn);
void afc_upload_dir(afc_client_t afc, QString &path,  QString &afcpath);

#endif // UTILS_H
