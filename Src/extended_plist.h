#pragma once
#include <plist/plist.h>
#include <QJsonDocument>

QJsonObject PlistToJsonObject(plist_t node);
QJsonDocument PlistToJson(plist_t node);
QJsonArray PlistToJsonArray(plist_t node);
QJsonValue PlistDictToJsonValue(plist_t node);
QJsonValue PlistNodeToJsonValue(plist_t node);
QJsonValue PlistArrayToJsonValue(plist_t node);
