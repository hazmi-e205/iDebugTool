#include "appinfo.h"
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include "utils.h"

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
    m_request = new SimpleRequest();
}

AppInfo::~AppInfo()
{
    delete m_request;
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

void AppInfo::CheckUpdate(const std::function<void (QString,QString)>& callback)
{
    m_request->Get("https://api.github.com/repos/hazmi-e205/iDebugTool/releases", [this, callback](QNetworkReply::NetworkError error, QJsonDocument response)
    {
        if (error != QNetworkReply::NetworkError::NoError)
            return;

        quint64 version = VersionToUInt(AppInfo::GetVersion());
        QString changelog("");
        QJsonArray rawdata = response.array();
        foreach (auto data, rawdata)
        {
            QString body = data.toObject()["body"].toString();
            QString tagName = data.toObject()["tag_name"].toString();
            quint64 version_itr = VersionToUInt(tagName);
            if (version_itr > version)
            {
                changelog += "==============\n";
                changelog += tagName + "\n";
                changelog += "==============\n";
                changelog += body + "\n\n\n";
            }
        }

        if (changelog.isEmpty())
            callback("", "");
        else
            callback(changelog, "https://github.com/hazmi-e205/iDebugTool/releases");
    });
}
