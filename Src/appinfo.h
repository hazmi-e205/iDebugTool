#ifndef APPINFO_H
#define APPINFO_H

#include <QJsonDocument>
#include "simplerequest.h"

class AppInfo
{
public:
    AppInfo(QWidget *parent);
    ~AppInfo();
    QJsonDocument GetJson();
    QString GetName();
    QString GetFullname();
    QString GetVersion();
    void CheckUpdate(const std::function<void (QString,QString)>& callback);

private:
    SimpleRequest *m_request;
    QJsonDocument m_infoJson;
};

#endif // APPINFO_H
