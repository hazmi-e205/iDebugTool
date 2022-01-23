#ifndef UTILS_H
#define UTILS_H

#include <QJsonDocument>
#include <plist/plist.h>

QJsonDocument PlistToJson(plist_t node);
QJsonValue PlistDictToJsonValue(plist_t node);
QJsonValue PlistNodeToJsonValue(plist_t node);
QJsonValue PlistArrayToJsonValue(plist_t node);
QString Base64Encode(QString string);

#endif // UTILS_H
