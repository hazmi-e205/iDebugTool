#include "utils.h"
#include <libimobiledevice-glue/utils.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QByteArray>
#include <QBitArray>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <zip.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "simplerequest.h"
#include "userconfigs.h"
#include <iostream>
#include <string>
#include <zip.h>

QJsonObject PlistToJsonObject(plist_t node)
{
    QJsonObject jsonObject;
    plist_dict_iter it = nullptr;
    char* key = nullptr;
    plist_t subnode = nullptr;
    plist_dict_new_iter(node, &it);
    plist_dict_next_item(node, it, &key, &subnode);
    while (subnode)
    {
        jsonObject.insert(key, PlistNodeToJsonValue(subnode));
        free(key);
        key = nullptr;
        plist_dict_next_item(node, it, &key, &subnode);
    }
    free(it);
    return jsonObject;
}

QJsonDocument PlistToJson(plist_t node)
{
    QJsonDocument jsonDoc;
    plist_type t = plist_get_node_type(node);
    switch (t) {
    case PLIST_ARRAY:
        {
            QJsonArray arr = PlistToJsonArray(node);
            jsonDoc.setArray(arr);
        }
        break;

    default:
        jsonDoc.setObject(PlistToJsonObject(node));
        break;
    }

    return jsonDoc;
}

QJsonArray PlistToJsonArray(plist_t node)
{
    QJsonArray arr;
    int i, count;
    plist_t subnode = nullptr;

    count = plist_array_get_size(node);
    for (i = 0; i < count; i++) {
        subnode = plist_array_get_item(node, i);
        arr.insert(0, PlistToJsonObject(subnode));
    }
    return arr;
}

QString Base64Encode(QString string)
{
    QByteArray ba;
    ba.append(string.toUtf8());
    return ba.toBase64();
}

QJsonValue PlistNodeToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    char *s = nullptr;
    char *data = nullptr;
    double d;
    uint8_t b;
    uint64_t u = 0;
    struct timeval tv = { 0, 0 };

    plist_type t;

    if (!node)
        return jsonValue;

    t = plist_get_node_type(node);

    switch (t) {
    case PLIST_BOOLEAN:
        plist_get_bool_val(node, &b);
        jsonValue = b ? true : false;
        break;

    case PLIST_UINT:
        plist_get_uint_val(node, &u);
        jsonValue = (double)u;
        break;

    case PLIST_REAL:
        plist_get_real_val(node, &d);
        jsonValue = d;
        break;

    case PLIST_STRING:
        plist_get_string_val(node, &s);
        jsonValue = s;
        free(s);
        break;

    case PLIST_KEY:
        plist_get_key_val(node, &s);
        //fprintf(stream, "%s: ", s);
        free(s);
        break;

    case PLIST_DATA:
        plist_get_data_val(node, &data, &u);
        if (u > 0) {
            QString str = Base64Encode(data);
            jsonValue = str;
        }
        break;

    case PLIST_DATE:
        plist_get_date_val(node, (int32_t*)&tv.tv_sec, (int32_t*)&tv.tv_usec);
        {
            time_t ti = (time_t)tv.tv_sec + MAC_EPOCH;
            struct tm *btime = localtime(&ti);
            if (btime) {
                s = (char*)malloc(24);
                memset(s, 0, 24);
                if (strftime(s, 24, "%Y-%m-%dT%H:%M:%SZ", btime) <= 0) {
                    free (s);
                    s = nullptr;
                }
            }
        }
        if (s) {
            jsonValue = s;
            free(s);
        }
        break;

    case PLIST_ARRAY:
        jsonValue = PlistArrayToJsonValue(node);
        break;

    case PLIST_DICT:
        jsonValue = PlistDictToJsonValue(node);
        break;

    default:
        break;
    }

    return jsonValue;
}

QJsonValue PlistDictToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    plist_dict_iter it = nullptr;
    char* key = nullptr;
    plist_t subnode = nullptr;
    plist_dict_new_iter(node, &it);
    plist_dict_next_item(node, it, &key, &subnode);
    while (subnode)
    {
        QJsonObject jsonObject;
        jsonObject.insert(key, PlistNodeToJsonValue(subnode));
        jsonValue = jsonObject;
        free(key);
        key = nullptr;
        plist_dict_next_item(node, it, &key, &subnode);
    }
    free(it);
    return jsonValue;
}

QJsonValue PlistArrayToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    int i, count;
    plist_t subnode = nullptr;

    count = plist_array_get_size(node);
    QJsonArray arr;
    for (i = 0; i < count; i++) {
        subnode = plist_array_get_item(node, i);
        arr.push_back(PlistNodeToJsonValue(subnode));
    }
    jsonValue = arr;
    return jsonValue;
}

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len)
{
    struct zip_stat zs;
    struct zip_file *zfile;
    int zindex = zip_name_locate(zf, filename, locate_flags);

    *buffer = NULL;
    *len = 0;

    if (zindex < 0) {
        return -1;
    }

    zip_stat_init(&zs);

    if (zip_stat_index(zf, zindex, 0, &zs) != 0) {
        fprintf(stderr, "ERROR: zip_stat_index '%s' failed!\n", filename);
        return -2;
    }

    if (zs.size > 10485760) {
        fprintf(stderr, "ERROR: file '%s' is too large!\n", filename);
        return -3;
    }

    zfile = zip_fopen_index(zf, zindex, 0);
    if (!zfile) {
        fprintf(stderr, "ERROR: zip_fopen '%s' failed!\n", filename);
        return -4;
    }

    *buffer = (char*)malloc(zs.size);
    if (zs.size > LLONG_MAX || zip_fread(zfile, *buffer, zs.size) != (zip_int64_t)zs.size) {
        fprintf(stderr, "ERROR: zip_fread %llu bytes from '%s'\n", (uint64_t)zs.size, filename);
        free(*buffer);
        *buffer = NULL;
        zip_fclose(zfile);
        return -5;
    }
    *len = zs.size;
    zip_fclose(zfile);
    return 0;
}

int zip_get_app_directory(struct zip* zf, QString &path)
{
    int i = 0;
    int c = zip_get_num_files(zf);
    int len = 0;
    const char* name = NULL;

    /* look through all filenames in the archive */
    do {
        /* get filename at current index */
        name = zip_get_name(zf, i++, 0);
        if (name != NULL) {
            /* check if we have a "Payload/.../" name */
            len = strlen(name);
            if (!strncmp(name, "Payload/", 8) && (len > 8)) {
                /* skip hidden files */
                if (name[8] == '.')
                    continue;

                /* locate the second directory delimiter */
                const char* p = name + 8;
                do {
                    if (*p == '/') {
                        break;
                    }
                } while(p++ != NULL);

                /* try next entry if not found */
                if (p == NULL)
                    continue;

                /* copy filename */
                QFileInfo file_info(name);
                path = file_info.filePath().remove(file_info.fileName());
                break;
            }
        }
    } while(i < c);

    return 0;
}

void StringWithSpaces(QString &string, bool CapFirstOnly)
{
    QString temp;
    // Traverse the string
    for(int i=0; i < string.length(); i++)
    {
        // Convert to lowercase if its
        // an uppercase character
        if (string[i]>='A' && string[i]<='Z')
        {
            // add space before it
            // if its an uppercase character
            if (i != 0)
                temp += " ";

            // add the character
            if (CapFirstOnly && i != 0)
                temp += string[i].toLower();
            else
                temp += string[i];
        }

        // if lowercase character then just add
        else
            temp += string[i];
    }
    string = temp;
}

QString FindRegex(QString rawString, QString regex)
{
    QRegularExpression re(regex);
    QRegularExpressionMatch match = re.match(rawString);
    QStringList captured = match.capturedTexts();
    return captured.length() > 0 ? captured[0] : "";
}

bool IsInternetOn()
{
    SimpleRequest req;
    return req.IsInternetOn();
}

quint64 VersionToUInt(QString version_raw)
{
    std::array<int,5> version = {0,0,0,3,0};
    QStringList version_list;
    version_list << FindRegex(version_raw, "\\d+\\.\\d+\\.\\d+-[\\S]+\\d+");
    version_list << FindRegex(version_raw, "\\d+\\.\\d+\\.\\d+");
    version_list << FindRegex(version_raw, "\\d+\\.\\d+");

    foreach (QString version_str, version_list) {
        if (version_str.isEmpty())
            continue;

        QStringList versions = version_str.split(".");
        if (version_str.contains('-'))
        {
            QStringList splitted = version_str.split('-');

            if (splitted.at(1).contains("alpha"))
                version[3] = 0;
            else if (splitted.at(1).contains("beta"))
                version[3] = 1;
            else if (splitted.at(1).contains("rc"))
                version[3] = 2;

            QString last = FindRegex(splitted.at(1), "\\d+");
            if (!last.isEmpty())
                version[4] = last.toInt();
            versions = splitted.at(0).split(".");
        }
        for (int idx = 0; idx < versions.count(); idx++)
            version[idx] = versions[idx].toInt();
        break;
    }
    return (version[0] * 10000000000) + (version[1] * 10000000) + (version[2] * 10000) + (version[3] * 1000) + version[4];
}

QString UIntToVersion(quint64 version_int, bool full)
{
    quint64 major = version_int / 10000000000;
    quint64 minor = (version_int % 10000000000) / 10000000;
    quint64 patch = ((version_int % 10000000000) % 10000000) / 10000;
    quint64 micro = (((version_int % 10000000000) % 10000000) % 10000) / 1000;
    quint64 nano  = (((version_int % 10000000000) % 10000000) % 10000) % 1000;

    QString version = QString::number(major) + "." + QString::number(minor) + (patch != 0 ? ("." + QString::number(patch)) : "");
    if (full)
    {
        QString status = "";
        switch (micro)
        {
        case 0:
            status = "alpha";
            break;
        case 1:
            status = "beta";
            break;
        case 2:
            status = "rc";
            break;
        }

        if (nano > 0)
            status += QString::number(nano);
        version = QString::number(major) + "." + QString::number(minor) + QString::number(patch) + (status.isEmpty() ? "" : status);
    }
    return version;
}

QString GetDirectory(DIRECTORY_TYPE dirtype)
{
    switch(dirtype)
    {
    case DIRECTORY_TYPE::APP:
        return QCoreApplication::applicationDirPath();
    case DIRECTORY_TYPE::LOCALDATA:
        return QCoreApplication::applicationDirPath() + "/LocalData/";
    case DIRECTORY_TYPE::TEMP:
        return QCoreApplication::applicationDirPath() + "/LocalData/Temp/";
    case DIRECTORY_TYPE::DISKIMAGES:
        return QCoreApplication::applicationDirPath() + "/LocalData/DiskImages/";
    case DIRECTORY_TYPE::SCREENSHOT:
        return QCoreApplication::applicationDirPath() + "/LocalData/Screenshot/";
    case DIRECTORY_TYPE::CRASHLOGS:
        return QCoreApplication::applicationDirPath() + "/LocalData/Crashlogs/";
    case DIRECTORY_TYPE::SYMBOLICATED:
        return QCoreApplication::applicationDirPath() + "/LocalData/Symbolicated/";
    case DIRECTORY_TYPE::RECODESIGNED:
        return QCoreApplication::applicationDirPath() + "/LocalData/Recodesigned/";
    case DIRECTORY_TYPE::ZSIGN_TEMP:
        return QCoreApplication::applicationDirPath() + "/LocalData/ZSignTemp/";
    default:
        break;
    }
    return "";
}

bool zip_extract_all(QString input_zip, QString output_dir, std::function<void(int,int,QString)> callback)
{
    QDir().mkpath(output_dir);
    int err;
    char buf[100];
    zip *za = zip_open(input_zip.toUtf8().data(), 0, &err);
    if (za == nullptr) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        callback(0, 0, QString::asprintf("Can't open zip archive `%s': %s", input_zip.toUtf8().data(), buf));
        return false;
    }

    struct FileItem {
        QString filepath;
        quint64 size;
        bool isdir;
    };

    QMap<int, FileItem> list_items;
    for (int idx = 0; idx < zip_get_num_entries(za, 0); idx++) {
        struct zip_stat sb;
        if (zip_stat_index(za, idx, 0, &sb) == 0) {
            size_t len = strlen(sb.name);
            FileItem item;
            item.filepath = QString(sb.name);
            item.size = sb.size;
            item.isdir = (sb.name[len - 1] == '/') ? true : false;
            list_items[idx] = item;
        }
    }

    for (int idx = 0; idx < list_items.count(); idx++) {
        FileItem item = list_items[idx];
        QString target = output_dir + "/" + QString(item.filepath);
        if (item.isdir) {
            callback(idx + 1, list_items.count(), QString("Creating `%1'...").arg(target));
            QDir().mkpath(target);
        }
        else
        {
            callback(idx + 1, list_items.count(), QString("Extracting `%1'...").arg(target));
            zip_file *zf = zip_fopen_index(za, idx, 0);
            if (!zf) {
                callback(idx + 1, list_items.count(), QString("Failed to load `%1' from zip!").arg(item.filepath));
                return false;
            }

            QFile file(target);
            if (!file.open(QIODevice::WriteOnly)) {
                callback(idx + 1, list_items.count(), "Error creating decrypted file!");
                return false;
            }
            else
            {
                char buff[1024];
                zip_uint64_t sum = 0;
                while (sum != item.size) {
                    zip_int64_t len = zip_fread(zf, static_cast<void*>(buff), 1024);
                    if (len > 0) {
                        file.write(buff, len);
                        sum += static_cast<zip_uint64_t>(len);
                    }
                    else
                    {
                        callback(idx + 1, list_items.count(), "Error creating decrypted file (2)!");
                        break;
                    }
                }
                file.close();
            }
            zip_fclose(zf);
        }
    }
    zip_close(za);
    return true;
}

std::function<void(int,int,QString)> callback_tmp = NULL;
void zip_progress_callback(int current, int total, const char* filename)
{
    if (callback_tmp)
        callback_tmp(current, total, QString::asprintf("Packing '%s' to package file...", filename));
}

bool zip_directory(QString input_dir, QString output_filename, std::function<void(int,int,QString)> callback)
{
    QString output_dir = QFileInfo(output_filename).absolutePath();
    QDir().mkpath(output_dir);
    int errorp;
    char buf[100];
    zip *zipper = zip_open(output_filename.toUtf8().data(), ZIP_CREATE | ZIP_EXCL, &errorp);
    if (zipper == nullptr) {
        zip_error_to_str(buf, sizeof(buf), errorp, errno);
        callback(0, 0, QString::asprintf("Can't create zip archive `%s': %s", output_filename.toUtf8().data(), buf));
        return false;
    }

    QStringList list_dirs, list_files;
    QDir dir(input_dir);
    QDirIterator it_dir(input_dir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_dir.hasNext())
        list_dirs << it_dir.next();

    QDirIterator it_file(input_dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it_file.hasNext())
        list_files << it_file.next();

    int idx = 0;
    int total = list_dirs.count() + list_files.count();
    foreach (QString dir_name, list_dirs)
    {
        idx++;
        QString relativepath = dir.relativeFilePath(dir_name);
        callback(idx, total, QString::asprintf("Adding `%s' to archive...", relativepath.toUtf8().data()));
        if (zip_add_dir(zipper, relativepath.toUtf8().data()) < 0)
        {
            callback(idx, total, QString::asprintf("Can't add `%s' to archive : %s", relativepath.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
    }
    foreach (QString file_name, list_files)
    {
        idx++;
        QString relativepath = dir.relativeFilePath(file_name);
        //callback(idx, total, QString::asprintf("Adding `%s' to archive...", relativepath.toUtf8().data()));

        zip_source *source = zip_source_file(zipper, file_name.toUtf8().data(), 0, 0);
        if (source == nullptr)
        {
            zip_close(zipper);
            callback(idx, total, QString::asprintf("Can't load `%s' : %s", file_name.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
        if (zip_add(zipper, relativepath.toUtf8().data(), source) < 0)
        {
            zip_source_free(source);
            zip_close(zipper);
            callback(idx, total, QString::asprintf("Can't add `%s' to archive : %s", relativepath.toUtf8().data(), zip_strerror(zipper)));
            return false;
        }
    }
    callback_tmp = callback;
    zip_close_with_callback(zipper, zip_progress_callback);
    callback_tmp = NULL;
    return true;
}

QStringList FindFiles(QString dir, QStringList criteria)
{
    QStringList files;
    QDirIterator it(dir, criteria, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
        files << it.next();
    return files;
}

QStringList FindDirs(QString dir, QStringList criteria)
{
    QStringList dirs;
    QDirIterator it(dir, criteria, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
        dirs << it.next();
    return dirs;
}

bool FilterVersion(QStringList &versions, QString version)
{
    QStringList final_list;
    quint64 selected_version = VersionToUInt(version);
    for (const auto& _version : versions)
    {
        quint64 current_version = VersionToUInt(_version);
        if (selected_version == current_version)
            final_list.append(_version);
    }
    versions = final_list;
    return final_list.length() > 0;
}

QString ParseVersion(QString version_raw)
{
    QString version_str = FindRegex(version_raw, "\\d+\\.\\d+\\.\\d+");
    version_str = version_str.isEmpty() ? FindRegex(version_raw, "\\d+\\.\\d+") : version_str;
    return version_str;
}

QString ShowBrowseDialog(BROWSE_TYPE browsetype, const QString &titleType, QWidget *parent, const QString &filter)
{
    QString last_dir = UserConfigs::Get()->GetData("Last" + titleType + "Dir", "");
    QString result, result_dir;

    switch (browsetype)
    {
    case BROWSE_TYPE::OPEN_FILE:
        result = result_dir = QFileDialog::getOpenFileName(parent, "Choose " + titleType + "...", last_dir, filter);
        break;

    case BROWSE_TYPE::SAVE_FILE:
        result = result_dir = QFileDialog::getSaveFileName(parent, "Save " + titleType + "...", last_dir, filter);
        break;

    case BROWSE_TYPE::OPEN_DIR:
        result = result_dir = QFileDialog::getExistingDirectory(parent, "Choose " + titleType + "...", last_dir);
        break;
    }

    if (!result_dir.isEmpty())
        UserConfigs::Get()->SaveData("Last" + titleType + "Dir", result_dir.mid(0, result_dir.length() - QFileInfo(result_dir).fileName().length() - 1));

    return result;
}

QString BytesToString(uint32_t bytes)
{
    float num = (float)bytes;
    QStringList list;
    list << "KB" << "MB" << "GB" << "TB";

    QStringListIterator i(list);
    QString unit("bytes");

    while(num >= 1024.0 && i.hasNext())
    {
        unit = i.next();
        num /= 1024.0;
    }
    return QString().setNum(num, 'f', 2) + " " + unit;
}

void MassStylesheet(STYLE_TYPE styleType, QList<QWidget *> widgets)
{
    QString stylesheet = "";
    switch (styleType)
    {
    case STYLE_TYPE::ROUNDED_BUTTON_LIGHT:
        stylesheet = QString()
                + "QPushButton {"
                + "     border: 1px solid rgba(0, 0, 0, 50);"
                + "     border-radius: 10px;"
                + "     padding: 4px;"
                + "     padding-right: 10px;"
                + "     padding-left: 10px;"
                + "     background-color: rgba(255, 255, 255, 150);"
                + "     color: rgb(68, 68, 68);"
                + "}"
                + "QPushButton:hover {"
                + "     border: 1px solid rgb(0, 170, 255);"
                + "}"
                + "QPushButton:focus {"
                + "     border: 1px solid rgb(85, 170, 255);"
                + "}";
        break;

    case STYLE_TYPE::ROUNDED_EDIT_LIGHT:
        stylesheet = QString()
                + "QLineEdit {"
                + "     border: 1px solid rgba(0, 0, 0, 50);"
                + "     border-radius: 10px;"
                + "     padding: 4px;"
                + "     background-color: rgba(255, 255, 255, 100);"
                + "     color: rgb(68, 68, 68);"
                + "}"
                + "QLineEdit:hover {"
                + "     border: 1px solid rgb(0, 170, 255);"
                + "}"
                + "QLineEdit:focus {"
                + "     border: 1px solid rgb(85, 170, 255);"
                + "     background-color: rgba(255, 255, 255, 200);"
                + "}";
        break;

    case STYLE_TYPE::ROUNDED_COMBOBOX_LIGHT:
        stylesheet = QString()
                + "QComboBox {"
                + "     border: 1px solid rgba(0, 0, 0, 50);"
                + "     border-radius: 10px;"
                + "     padding: 4px;"
                + "     background-color: rgba(255, 255, 255, 100);"
                + "     color: rgb(68, 68, 68);"
                + "}"
                + "QComboBox::drop-down {"
                + "     border: 0px;"
                + "}"
                + "QComboBox::down-arrow {"
                + "     image: url(:/res/Assets/arrow-down.png);"
                + "     width: 12px;"
                + "     height: 10px;"
                + "}"
                + "QComboBox:hover {"
                + "     border: 1px solid rgb(0, 170, 255);"
                + "}"
                + "QComboBox:focus {"
                + "     border: 1px solid rgb(85, 170, 255);"
                + "     background-color: rgba(255, 255, 255, 200);"
                + "}";
        break;
    }

    foreach (auto widget, widgets)
    {
        widget->setStyleSheet(stylesheet);
    }
}

void DecorateSplitter(QSplitter* splitter, int index)
{
    const int gripLength = 50;
    const int gripWidth = 1;
    const int grips = 4;

    splitter->setOpaqueResize(false);
    splitter->setChildrenCollapsible(false);

    splitter->setHandleWidth(7);
    QSplitterHandle* handle = splitter->handle(index);
    Qt::Orientation orientation = splitter->orientation();
    QHBoxLayout* layout = new QHBoxLayout(handle);
    layout->setSpacing(0);
    //layout->setMargin(0);

    if (orientation == Qt::Horizontal)
    {
        for (int i=0;i<grips;++i)
        {
            QFrame* line = new QFrame(handle);
            line->setMinimumSize(gripWidth, gripLength);
            line->setMaximumSize(gripWidth, gripLength);
            line->setFrameShape(QFrame::StyledPanel);
            layout->addWidget(line);
        }
    }
    else
    {
        //this will center the vertical grip
        //add a horizontal spacer
        layout->addStretch();
        //create the vertical grip
        QVBoxLayout* vbox = new QVBoxLayout;
        for (int i=0;i<grips;++i)
        {
            QFrame* line = new QFrame(handle);
            line->setMinimumSize(gripLength, gripWidth);
            line->setMaximumSize(gripLength, gripWidth);
            line->setFrameShape(QFrame::StyledPanel);
            vbox->addWidget(line);
        }
        layout->addLayout(vbox);
        //add another horizontal spacer
        layout->addStretch();
    }
}
