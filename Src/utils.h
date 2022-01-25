#ifndef UTILS_H
#define UTILS_H

#include <QJsonDocument>
#include <plist/plist.h>
#include "logpacket.h"

// function
QJsonDocument PlistToJson(plist_t node);
QJsonValue PlistDictToJsonValue(plist_t node);
QJsonValue PlistNodeToJsonValue(plist_t node);
QJsonValue PlistArrayToJsonValue(plist_t node);
QString Base64Encode(QString string);
bool ParseSystemLogs(char &in, LogPacket &out);

#endif // UTILS_H
