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
    void ShowDialog();

private:
    Ui::ImageMounter *ui;
    SimpleRequest *m_request;
    QMap<quint64,QString> m_imagelinks;

private slots:
    void OnImageClicked();
    void OnSignatureClicked();
    void OnMountClicked();
    void OnDownloadMountClicked();
    void OnMounterStatusChanged(QString messages);
};

#endif // IMAGEMOUNTER_H
