#ifndef SIMPLEREQUEST_H
#define SIMPLEREQUEST_H

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QNetworkProxy>

class SimpleRequest : public QObject
{
    Q_OBJECT
public:
    enum class RequestCmd
    {
        REQ_NONE,
        REQ_DOWNLOAD,
        REQ_GET
    };

    enum class RequestState
    {
        STATE_NONE,
        STATE_PROGRESS,
        STATE_FINISH,
        STATE_ERROR
    };

    struct RequestQueue
    {
        RequestQueue(){}
        RequestQueue(RequestCmd cmd, QString url, std::function<void(RequestState,float,QNetworkReply::NetworkError,QByteArray)> clbk) :
            command(cmd), url(url), dlcallback(clbk) {}
        RequestQueue(RequestCmd cmd, QString url, std::function<void (QNetworkReply::NetworkError, QJsonDocument)> clbk) :
            command(cmd), url(url), callback(clbk) {}
        RequestCmd command = RequestCmd::REQ_NONE;
        QString url = "";
        std::function<void (QNetworkReply::NetworkError,QJsonDocument)> callback = nullptr;
        std::function<void(RequestState,int,QNetworkReply::NetworkError,QByteArray)> dlcallback = nullptr;
    };

    SimpleRequest();
    ~SimpleRequest();
    void Download(QString url, const std::function<void(RequestState,int,QNetworkReply::NetworkError,QByteArray)>& responseCallback = nullptr);
    void Get(QString url, const std::function<void(QNetworkReply::NetworkError,QJsonDocument)>& responseCallback = nullptr);
    static bool IsInternetOn();
    static QByteArray PostSync(const QString& url, const QByteArray& data, const QString& contentType, QString* errorStr = nullptr);

signals:
    void RequestResponse(QNetworkReply::NetworkError errorcode, QJsonDocument datajson);
    void DownloadResponse(SimpleRequest::RequestState state, int progress, QNetworkReply::NetworkError error, QByteArray data);

private:
    void DoNextQueue();
    QNetworkAccessManager *m_manager;
    QNetworkReply* m_reply;
    std::function<void (QNetworkReply::NetworkError, QJsonDocument)> m_callback;
    std::function<void(RequestState,int,QNetworkReply::NetworkError,QByteArray)> m_dlcallback;
    std::vector<RequestQueue> m_getQueue;
    int m_progress;
    bool m_stillrunning;
    QByteArray m_downloadedData;

private slots:
    void Response(QNetworkReply *reply);
    void DownloadProgress(qint64 read, qint64 total);
    void DownloadError(QNetworkReply::NetworkError error);
    void DownloadFinish();
    void ReadyToRead();
};

#endif // SIMPLEREQUEST_H
