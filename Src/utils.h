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
void StringWithSpaces(QString &string, bool CapFirstOnly = false);
QString FindRegex(QString rawString, QString regex);
bool IsInternetOn();
QString ParseVersion(QString version_raw);
quint64 VersionToUInt(QString version_raw);
QString UIntToVersion(quint64 version_int, bool full = false);
bool FilterVersion(QStringList& versions, QString version);
QStringList FindFiles(QString dir, QStringList criteria);
QString BytesToString(uint32_t bytes);

enum DIRECTORY_TYPE
{
    APP,
    LOCALDATA,
    TEMP,
    DISKIMAGES,
    SCREENSHOT,
    CRASHLOGS,
    SYMBOLICATED,
    RECODESIGNED
};
QString GetDirectory(DIRECTORY_TYPE dirtype);

enum BROWSE_TYPE
{
    OPEN_FILE,
    SAVE_FILE,
    OPEN_DIR
};
QString ShowBrowseDialog(BROWSE_TYPE browsetype, const QString& titleType, QWidget *parent = nullptr, const QString& filter = QString());

enum STYLE_TYPE
{
    ROUNDED_BUTTON_LIGHT,
    ROUNDED_EDIT_LIGHT,
    ROUNDED_COMBOBOX_LIGHT
};
void MassStylesheet(STYLE_TYPE styleType, QList<QWidget*> widgets);

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len);
int zip_get_app_directory(struct zip* zf, QString &path);
bool zip_extract_all(struct zip *zf, QString dist);

#endif // UTILS_H
