#include "appledb.h"

#include "simplerequest.h"
#include "utils.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

AppleDB::AppleDB(QObject* parent)
    : QObject(parent)
    , m_request(new SimpleRequest())
    , m_localFilePath(GetDirectory(DIRECTORY_TYPE::LOCALDATA) + LOCAL_FILE_NAME)
    , m_isRefreshing(false)
{
    LoadLocal();
}

AppleDB::~AppleDB()
{
    delete m_request;
}

void AppleDB::EnsureDownloadedAtLaunch()
{
    if (!QFile::exists(m_localFilePath))
        RefreshFromNetwork(false);
}

QString AppleDB::ResolveProductType(const QString& productType, const std::function<void(const QString&)>& onResolved)
{
    if (productType.isEmpty())
        return "";

    const QString display = BuildDisplayName(productType);
    if (display != productType)
        return display;

    if (onResolved)
        m_pendingCallbacks[productType].append(onResolved);
    RefreshFromNetwork(true);
    return EstimateSeries(productType);
}

void AppleDB::LoadLocal()
{
    QFile file(m_localFilePath);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly))
        return;

    ParseAndStore(file.readAll(), false);
}

void AppleDB::RefreshFromNetwork(bool force)
{
    if (m_isRefreshing)
        return;
    if (!force && QFile::exists(m_localFilePath))
        return;

    m_isRefreshing = true;
    m_request->Download(REMOTE_URL,
                        [this](SimpleRequest::RequestState state, int, QNetworkReply::NetworkError error, QByteArray data)
                        {
                            if (state != SimpleRequest::RequestState::STATE_FINISH &&
                                state != SimpleRequest::RequestState::STATE_ERROR)
                            {
                                return;
                            }

                            if (state == SimpleRequest::RequestState::STATE_FINISH && error == QNetworkReply::NoError)
                                ParseAndStore(data, true);

                            m_isRefreshing = false;
                            ResolvePending();
                        });
}

bool AppleDB::ParseAndStore(const QByteArray& jsonData, bool persistToDisk)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return false;

    QHash<QString, QString> parsed;
    const QJsonArray devices = document.array();
    for (const QJsonValue& value : devices)
    {
        if (!value.isObject())
            continue;

        const QJsonObject device = value.toObject();
        const QString name = device["name"].toString();
        if (name.isEmpty())
            continue;

        const QJsonValue identifierValue = device["identifier"];
        if (identifierValue.isArray())
        {
            for (const QJsonValue& identifier : identifierValue.toArray())
            {
                const QString productType = identifier.toString();
                if (!productType.isEmpty())
                    parsed[productType] = name;
            }
        }
        else
        {
            const QString productType = identifierValue.toString();
            if (!productType.isEmpty())
                parsed[productType] = name;
        }
    }

    if (parsed.isEmpty())
        return false;

    m_deviceNameByIdentifier = parsed;
    if (persistToDisk)
    {
        QDir().mkpath(GetDirectory(DIRECTORY_TYPE::LOCALDATA));
        QSaveFile file(m_localFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(jsonData);
            file.commit();
        }
    }
    return true;
}

QString AppleDB::BuildDisplayName(const QString& productType) const
{
    const QString resolved = m_deviceNameByIdentifier.value(productType);
    if (resolved.isEmpty())
        return productType;
    return resolved;
}

QString AppleDB::EstimateSeries(const QString& productType) const
{
    QRegularExpressionMatch match = QRegularExpression("^iPhone(\\d+),\\d+$").match(productType);
    if (match.hasMatch())
    {
        const int hardwareSeries = match.captured(1).toInt();
        int estimatedSeries = hardwareSeries;
        if (hardwareSeries >= 11)
            estimatedSeries = hardwareSeries - 1;
        else if (hardwareSeries >= 8)
            estimatedSeries = hardwareSeries - 2;

        return QString("Estimated iPhone %1 series").arg(estimatedSeries);
    }
    return "Unknown model";
}

void AppleDB::ResolvePending()
{
    const auto pendingMap = m_pendingCallbacks;
    m_pendingCallbacks.clear();

    for (auto it = pendingMap.constBegin(); it != pendingMap.constEnd(); ++it)
    {
        const QString productType = it.key();
        const QString display = BuildDisplayName(productType);
        const QString resolved = display == productType ? EstimateSeries(productType) : display;
        for (const auto& callback : it.value())
            callback(resolved);
    }
}
