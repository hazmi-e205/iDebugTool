#ifndef USERCONFIGS_H
#define USERCONFIGS_H

#include <QJsonDocument>

class UserConfigs
{
private:
    static UserConfigs *m_instance;
    QJsonDocument m_json;

public:
    static UserConfigs *Get();
    static void Destroy();
    UserConfigs();

    void SaveData(QString key, QString value);
    QString GetData(QString key, QString defaultvalue);
    void SaveToFile();
    void ReadFromFile();
};

#endif // USERCONFIGS_H
