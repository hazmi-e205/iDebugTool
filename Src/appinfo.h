#ifndef APPINFO_H
#define APPINFO_H

#include <QJsonDocument>

class AppInfo
{
public:
    AppInfo(QWidget *parent);
    QJsonDocument GetJson();
    QString GetName();
    QString GetFullname();
    QString GetVersion();

private:
    QJsonDocument m_infoJson;
};

#endif // APPINFO_H
