#ifndef UTILS_H
#define UTILS_H
#include <QWidget>

QString Base64Encode(QString string);
void StringWithSpaces(QString &string, bool CapFirstOnly = false);
QString FindRegex(QString rawString, QString regex);
QString ParseVersion(QString version_raw);
quint64 VersionToUInt(QString version_raw);
QString UIntToVersion(quint64 version_int, bool full = false);
bool FilterVersion(QStringList& versions, QString version);
QStringList FindFiles(QString dir, QStringList criteria);
QStringList FindDirs(QString dir, QStringList criteria);
QString BytesToString(uint32_t bytes);
bool CopyFolder(QString input_dir, QString output_dir, std::function<void(int,int,QString)> callback);

enum DIRECTORY_TYPE
{
    APP,
    LOCALDATA,
    TEMP,
    DISKIMAGES,
    SCREENSHOT,
    CRASHLOGS,
    SYMBOLICATED,
    ZSIGN_TEMP,
    RECODESIGNED
};
QString GetDirectory(DIRECTORY_TYPE dirtype);
QString GetBaseDirectory(QString inpath);

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
void DecorateSplitter(QWidget* splitterWidget, int index);
void CheckDrMinGWReports(QString filename, std::function<void(QString,int)> callback);

#endif // UTILS_H
