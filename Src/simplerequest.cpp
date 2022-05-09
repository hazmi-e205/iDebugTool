#include "simplerequest.h"

SimpleRequest::SimpleRequest() : m_manager(nullptr), m_reply(nullptr), m_callback(nullptr), m_dlcallback(nullptr), m_progress(0.f), m_stillrunning(false)
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(Response(QNetworkReply*)));
}

SimpleRequest::~SimpleRequest()
{
    m_callback = nullptr;
    m_dlcallback = nullptr;
    delete m_reply;
    delete m_manager;
}

void SimpleRequest::Download(QString url, const std::function<void (RequestState, int, QNetworkReply::NetworkError, QByteArray)> &responseCallback)
{
    if (!m_stillrunning)
    {
        m_downloadedData.clear();
        m_progress = 0;
        m_dlcallback = responseCallback;

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        m_reply = m_manager->get(request);
        connect(m_reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(DownloadError(QNetworkReply::NetworkError)));
        connect(m_reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(DownloadProgress(qint64,qint64)));
        connect(m_reply, SIGNAL(finished()), this, SLOT(DownloadFinish()));
        connect(m_reply, SIGNAL(readyRead()), this, SLOT(ReadyToRead()));
        m_stillrunning = true;
    }
    else
    {
        m_getQueue.push_back(RequestQueue(RequestCmd::REQ_DOWNLOAD, url, responseCallback));
    }
}

void SimpleRequest::Get(QString url, const std::function<void (QNetworkReply::NetworkError, QJsonDocument)> &responseCallback)
{
    if (!m_stillrunning)
    {
        m_callback = responseCallback;

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        m_manager->get(request);
        m_stillrunning = true;
    }
    else
    {
        m_getQueue.push_back(RequestQueue(RequestCmd::REQ_GET, url, responseCallback));
    }
}

void SimpleRequest::DoNextQueue()
{
    if (m_getQueue.size() > 0)
    {
        switch (m_getQueue.begin()->command)
        {
            case RequestCmd::REQ_DOWNLOAD:
                Download(m_getQueue.begin()->url, m_getQueue.begin()->dlcallback);
                break;

            case RequestCmd::REQ_GET:
                Get(m_getQueue.begin()->url, m_getQueue.begin()->callback);
                break;

            default:
                break;
        }
        m_getQueue.erase(m_getQueue.begin());
    }
}

void SimpleRequest::Response(QNetworkReply *reply)
{
    m_stillrunning = false;
    if (m_callback)
    {
        m_callback(reply->error(), QJsonDocument::fromJson(reply->readAll()));
        m_callback = nullptr;
    }
    else
    {
        emit RequestResponse(reply->error(), QJsonDocument::fromJson(reply->readAll()));
    }

    DoNextQueue();
}

void SimpleRequest::DownloadProgress(qint64 read, qint64 total)
{
    m_progress = (int)(((float)read / (float)total) * 100.f);
    if (m_dlcallback)
    {
        m_dlcallback(RequestState::STATE_PROGRESS, m_progress, m_reply->error(), nullptr);
    }
    else
    {
        emit DownloadResponse(RequestState::STATE_PROGRESS, m_progress, m_reply->error(), nullptr);
    }
}

void SimpleRequest::DownloadError(QNetworkReply::NetworkError error)
{
    m_stillrunning = false;
    if (m_dlcallback)
    {
        m_dlcallback(RequestState::STATE_ERROR, m_progress, error, nullptr);
        m_dlcallback = nullptr;
    }
    else
    {
        emit DownloadResponse(RequestState::STATE_ERROR, m_progress, error, nullptr);
    }
    DoNextQueue();
}

void SimpleRequest::DownloadFinish()
{
    m_stillrunning = false;
    m_progress = 100;
    if (m_dlcallback)
    {
        m_dlcallback(RequestState::STATE_FINISH, m_progress, m_reply->error(), m_downloadedData);
        m_dlcallback = nullptr;
    }
    else
    {
        emit DownloadResponse(RequestState::STATE_FINISH, m_progress, m_reply->error(), m_downloadedData);
    }
    DoNextQueue();
}

void SimpleRequest::ReadyToRead()
{
    m_downloadedData.append(m_reply->readAll());
}
