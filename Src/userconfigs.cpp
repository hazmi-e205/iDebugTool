#include "userconfigs.h"
#include "utils.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QJsonArray>

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

void UserConfigs::SaveData(QString key, QStringList values)
{
    QJsonArray array;
    foreach (const auto& value, values)
        array.append(value);
    SaveData(key, array);
}

QString UserConfigs::GetData(QString key, QString defaultvalue)
{
    ReadFromFile();
    QString data = m_json[key].toString(defaultvalue);
    if (!data.isEmpty())
        return data;
    return defaultvalue;
}

QString UserConfigs::GetData(QString key, const char *defaultvalue)
{
    return GetData(key, QString(defaultvalue));
}

bool UserConfigs::GetData(QString key, bool defaultvalue)
{
    ReadFromFile();
    return m_json[key].toBool(defaultvalue);
}

int UserConfigs::GetData(QString key, int defaultvalue)
{
    ReadFromFile();
    return m_json[key].toInt(defaultvalue);
}

QJsonObject UserConfigs::GetData(QString key, QJsonObject defaultvalue)
{
    ReadFromFile();
    return m_json[key].toObject(defaultvalue);
}

QStringList UserConfigs::GetData(QString key, QStringList defaultvalue)
{
    ReadFromFile();
    QJsonArray defaultArray;
    foreach (const QString& value, defaultvalue)
        defaultArray.append(value);

    defaultArray = m_json[key].toArray(defaultArray);
    defaultvalue.clear();
    foreach (const auto& value, defaultArray)
        defaultvalue << value.toString();
    return defaultvalue;
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
