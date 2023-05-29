#ifndef USERCONFIGS_H
#define USERCONFIGS_H

#include <QJsonDocument>
#include <QJsonObject>

class UserConfigs
{
private:
    static UserConfigs *m_instance;
    QJsonDocument m_json;

public:
    static UserConfigs *Get();
    static void Destroy();
    UserConfigs();

    void SaveData(QString key, QJsonValue value);
    void SaveData(QString key, QStringList values);
    QString GetData(QString key, QString defaultvalue);
    QString GetData(QString key, const char* defaultvalue);
    bool GetData(QString key, bool defaultvalue);
    int GetData(QString key, int defaultvalue);
    QJsonObject GetData(QString key, QJsonObject defaultvalue);
    QStringList GetData(QString key, QStringList defaultvalue);
    void SaveToFile();
    void ReadFromFile();
};

#endif // USERCONFIGS_H
