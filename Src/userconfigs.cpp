#include "userconfigs.h"
#include "utils.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>

UserConfigs *UserConfigs::m_instance = nullptr;
UserConfigs *UserConfigs::Get()
{
    if(!m_instance)
        m_instance = new UserConfigs();
    return m_instance;
}

void UserConfigs::Destroy()
{
    if(m_instance)
        delete m_instance;
}

UserConfigs::UserConfigs()
{
}

void UserConfigs::SaveData(QString key, QJsonValue value)
{
    ReadFromFile();
    QJsonObject jobject = m_json.object();
    jobject.insert(key, value);
    m_json.setObject(jobject);
    SaveToFile();
}

QString UserConfigs::GetData(QString key, QString defaultvalue)
{
    ReadFromFile();
    QString data = m_json[key].toString(defaultvalue);
    if (!data.isEmpty())
        return data;
    return defaultvalue;
}

bool UserConfigs::GetBool(QString key, bool defaultvalue)
{
    ReadFromFile();
    return m_json[key].toBool(defaultvalue);
}

int UserConfigs::GetInt(QString key, int defaultvalue)
{
    ReadFromFile();
    return m_json[key].toInt(defaultvalue);
}

void UserConfigs::SaveToFile()
{
    QByteArray bytes = m_json.toJson(QJsonDocument::Indented);
    QString dir = GetDirectory(DIRECTORY_TYPE::LOCALDATA);
    QDir().mkpath(dir);
    QFile file(dir + "configs.json");
    if(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream iStream(&file);
        iStream.setEncoding(QStringConverter::Encoding::Utf8);
        iStream << bytes;
        file.close();
    }
}

void UserConfigs::ReadFromFile()
{
    if (!m_json.isEmpty())
        return;

    QFile file(GetDirectory(DIRECTORY_TYPE::LOCALDATA) + "configs.json");
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray bytes = file.readAll();
        file.close();

        QJsonParseError jsonError;
        m_json = QJsonDocument::fromJson(bytes, &jsonError);
    }
}
