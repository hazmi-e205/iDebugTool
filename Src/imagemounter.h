#ifndef IMAGEMOUNTER_H
#define IMAGEMOUNTER_H

#include <QDialog>
#include <QJsonArray>
#include "simplerequest.h"

namespace Ui {
class ImageMounter;
}

class ImageMounter : public QDialog
{
    Q_OBJECT

public:
    explicit ImageMounter(QWidget *parent = nullptr);
    ~ImageMounter();
    void RefreshUI(bool fetchImages = true);
    void ShowDialog(bool closeAction = false);

private:
    enum class DOWNLOAD_TYPE
    {
        ARCHIVED_IMAGE,
        DIRECT_FILES
    };
    void DownloadImage(DOWNLOAD_TYPE downloadtype);

    enum class DOWNLOAD_STATE
    {
        IDLE,
        FETCH,
        DOWNLOAD,
        DONE
    };
    void ChangeDownloadState(DOWNLOAD_STATE downloadState);

    Ui::ImageMounter *ui;
    DOWNLOAD_STATE m_downloadState;
    SimpleRequest *m_request;
    QJsonDocument m_repoJson;
    QMap<QString,QString> m_downloadUrls;
    QString m_downloadurl, m_downloadout;
    DOWNLOAD_TYPE m_downloadtype;
    bool m_isCloseAction;

protected slots:
    void OnImageClicked();
    void OnSignatureClicked();
    void OnMountClicked();
    void OnDownloadMountClicked();
    void OnRepoChanged(QString messages);
    void OnMounterStatusChanged(QString messages);
    void OnDownloadResponse(SimpleRequest::RequestState req_state, int percentage, QNetworkReply::NetworkError error, QByteArray data);
};

#endif // IMAGEMOUNTER_H
