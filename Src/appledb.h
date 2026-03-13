#ifndef APPLEDB_H
#define APPLEDB_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

class SimpleRequest;

class AppleDB : public QObject
{
    Q_OBJECT

public:
    explicit AppleDB(QObject* parent = nullptr);
    ~AppleDB();

    void EnsureDownloadedAtLaunch();
    QString ResolveProductType(const QString& productType, const std::function<void(const QString&)>& onResolved = nullptr);

private:
    static constexpr const char* REMOTE_URL = "https://api.appledb.dev/device/main.json";
    static constexpr const char* LOCAL_FILE_NAME = "appledb-main.json";

    SimpleRequest* m_request;
    QString m_localFilePath;
    QHash<QString, QString> m_deviceNameByIdentifier;
    QHash<QString, QList<std::function<void(const QString&)>>> m_pendingCallbacks;
    bool m_isRefreshing;

    void LoadLocal();
    void RefreshFromNetwork(bool force);
    bool ParseAndStore(const QByteArray& jsonData, bool persistToDisk);
    QString BuildDisplayName(const QString& productType) const;
    QString EstimateSeries(const QString& productType) const;
    void ResolvePending();
};

#endif // APPLEDB_H
