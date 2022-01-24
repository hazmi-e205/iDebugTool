#include "appinfo.h"
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>

AppInfo::AppInfo(QWidget *parent)
{
    QFile mFile(":res/info.json");
    if(!mFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(parent, "Error", "Application detail info file missing!", QMessageBox::Ok);
    }
    QTextStream in(&mFile);
    m_infoJson = QJsonDocument::fromJson(in.readAll().toUtf8());
    mFile.close();
}

QJsonDocument AppInfo::GetJson()
{
    return m_infoJson;
}

QString AppInfo::GetName()
{
    return m_infoJson["project"].toString();
}

QString AppInfo::GetFullname()
{
    return m_infoJson["project"].toString() + " v" + GetVersion();
}

QString AppInfo::GetVersion()
{
    QString pStatus = m_infoJson["status"].toString();
    return m_infoJson["version"].toString() + (pStatus != "release" ? "-" + pStatus : "");
}
