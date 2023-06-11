#include "utils.h"
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
#include <QSplitter>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "userconfigs.h"
#include <iostream>
#include <string>

QString Base64Encode(QString string)
{
    QByteArray ba;
    ba.append(string.toUtf8());
    return ba.toBase64();
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
    QString result;

    switch (browsetype)
    {
    case BROWSE_TYPE::OPEN_FILE:
        result = QFileDialog::getOpenFileName(parent, "Choose " + titleType + "...", last_dir, filter);
        break;

    case BROWSE_TYPE::SAVE_FILE:
        result = QFileDialog::getSaveFileName(parent, "Save " + titleType + "...", last_dir, filter);
        break;

    case BROWSE_TYPE::OPEN_DIR:
        result = QFileDialog::getExistingDirectory(parent, "Choose " + titleType + "...", last_dir);
        break;
    }

    if (!result.isEmpty())
        UserConfigs::Get()->SaveData("Last" + titleType + "Dir", GetBaseDirectory(result));

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

void DecorateSplitter(QWidget* splitterWidget, int index)
{
    const int gripLength = 50;
    const int gripWidth = 1;
    const int grips = 4;

    QSplitter* splitter = (QSplitter*)splitterWidget;
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

bool CopyFolder(QString input_dir, QString output_dir, std::function<void (int, int, QString)> callback)
{
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
        QString targetpath = QString("%1/%2").arg(output_dir).arg(relativepath);
        callback(idx, total, QString("Creating `%1' to '%2'...").arg(relativepath).arg(output_dir));
        if (!QDir().mkpath(targetpath))
        {
            callback(idx, total, QString("Can't create `%1' to '%2'...").arg(relativepath).arg(output_dir));
            return false;
        }
    }
    foreach (QString file_name, list_files)
    {
        idx++;
        QString relativepath = dir.relativeFilePath(file_name);
        QString targetpath = QString("%1/%2").arg(output_dir).arg(relativepath);
        callback(idx, total, QString("Copying `%1' to '%2'...").arg(relativepath).arg(output_dir));
        if (!QFile(file_name).copy(targetpath))
        {
            callback(idx, total, QString("Can't copy `%1' to '%2'...").arg(relativepath).arg(output_dir));
            return false;
        }
    }
    return true;
}

QString GetBaseDirectory(QString inpath)
{
    QFileInfo info(inpath);
    QString abspath = info.absoluteFilePath();
    return abspath.mid(0, abspath.length() - info.fileName().length() - 1);
}

void CheckDrMinGWReports(QString filename, std::function<void (QString,int)> callback)
{
    QString filepath = GetDirectory(DIRECTORY_TYPE::APP) + "/" + filename;
    QFile file(filepath);
    file.open(QIODevice::ReadOnly);
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    int current_reports = content.count("Error occurred",Qt::CaseInsensitive);
    if (current_reports != UserConfigs::Get()->GetData(filename + "_count", 0))
    {
        UserConfigs::Get()->SaveData(filename + "_count", current_reports);
        callback(filepath, current_reports);
    }
}
