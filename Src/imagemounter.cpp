#include "imagemounter.h"
#include "devicebridge.h"
#include <QFileDialog>
#include <QSaveFile>
#include <QMessageBox>
#include <ui_imagemounter.h>
#include <zip.h>
#include "utils.h"

ImageMounter::ImageMounter(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::ImageMounter)
  , m_downloadState(DOWNLOAD_STATE::IDLE)
  , m_request(nullptr)
  , m_isCloseAction(false)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    connect(ui->imageBtn, SIGNAL(pressed()), this, SLOT(OnImageClicked()));
    connect(ui->signatureBtn, SIGNAL(pressed()), this, SLOT(OnSignatureClicked()));
    connect(ui->mountBtn, SIGNAL(pressed()), this, SLOT(OnMountClicked()));
    connect(ui->mount2Btn, SIGNAL(pressed()), this, SLOT(OnDownloadMountClicked()));
    connect(ui->repoBox, SIGNAL(currentTextChanged(QString)), this, SLOT(OnRepoChanged(QString)));
    connect(DeviceBridge::Get(), SIGNAL(MounterStatusChanged(QString)), this, SLOT(OnMounterStatusChanged(QString)));
    m_request = new SimpleRequest();
    connect(m_request, SIGNAL(DownloadResponse(SimpleRequest::RequestState,int,QNetworkReply::NetworkError,QByteArray)), this, SLOT(OnDownloadResponse(SimpleRequest::RequestState,int,QNetworkReply::NetworkError,QByteArray)));

    QFile mFile(":res/Assets/disk_repos.json");
    if(!mFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(parent, "Error", "Disk images repo file missing!", QMessageBox::Ok);
    }
    QTextStream in(&mFile);
    m_repoJson = QJsonDocument::fromJson(in.readAll().toUtf8());
    mFile.close();
}

ImageMounter::~ImageMounter()
{
    delete m_request;
    delete ui;
}

void ImageMounter::RefreshUI(bool fetchImages)
{
    QString OSVersion = DeviceBridge::Get()->GetDeviceInfo()["ProductVersion"].toString();
    ui->OSVersion->setText(OSVersion);
    auto mounted = DeviceBridge::Get()->GetMountedImages();
    if (mounted.length() > 0)
    {
        ui->Signatures->setText(mounted.at(0));

        if (m_isCloseAction) {
            close(); return;
        }
    }
    else
    {
        ui->Signatures->setText("No image mounted!");
    }

    if (IsInternetOn())
    {
        ui->onlineTab->setEnabled(true);
        ui->tabWidget->setCurrentWidget(ui->onlineTab);
        if (fetchImages)
        {
            for (const auto& owner : m_repoJson.object().keys())
            {
                ui->logField->append("Fetch from '" + m_repoJson[owner].toObject()["git_url"].toString() + "'...");
                if (ui->repoBox->findText(owner) < 0)
                    ui->repoBox->addItem(owner);

                m_request->Get(m_repoJson[owner].toObject()["repo_url"].toString(), [this, owner](QNetworkReply::NetworkError responseCode, QJsonDocument ResponseData){
                    if (responseCode == QNetworkReply::NetworkError::NoError)
                    {
                        ui->logField->append("Parsing data from '" + owner + "'...");
                        bool repo_archived = m_repoJson[owner].toObject()["is_archived"].toBool();
                        QJsonObject imagesJson;
                        auto rawdata = ResponseData.array();
                        for (int idx = 0; idx < rawdata.count(); ++idx)
                        {
                            QString nameStr = rawdata.at(idx)["name"].toString();
                            imagesJson.insert(ParseVersion(nameStr), repo_archived ? rawdata.at(idx)["download_url"].toString() : rawdata.at(idx)["url"].toString());
                        }

                        QJsonObject repoJson = m_repoJson.object();
                        QJsonObject newData = m_repoJson[owner].toObject();
                        newData.insert("images", imagesJson);
                        repoJson.insert(owner, newData);
                        m_repoJson.setObject(repoJson);

                        ui->logField->append("Data from '" + owner + "' converted.");
                        OnRepoChanged(ui->repoBox->currentText());
                    }
                });
            }
        }
    }
    else
    {
        ui->onlineTab->setEnabled(false);
        ui->tabWidget->setCurrentWidget(ui->offlineTab);
    }
}

void ImageMounter::OnRepoChanged(QString messages)
{
    ui->imageBox->clear();
    QJsonObject available_images = m_repoJson[messages].toObject()["images"].toObject();
    quint64 os_version = VersionToUInt(DeviceBridge::Get()->GetDeviceInfo()["ProductVersion"].toString());
    QMap<quint64,QString> imagelinks;

    // sort data
    foreach (const auto& version_str, available_images.keys())
    {
        imagelinks[VersionToUInt(version_str)] = available_images[version_str].toString();
    }

    // add item and recommendation
    QString version_selected;
    foreach (const quint64 &image_ver, imagelinks.keys())
    {
        ui->imageBox->addItem(UIntToVersion(image_ver));
        if (image_ver <= os_version)
        {
            version_selected = UIntToVersion(image_ver);

            if(image_ver == os_version)
                ui->imageStatus->setText("Choose image same as your iOS version (" + version_selected + ")");
            else
                ui->imageStatus->setText("Choose closest version, sometime it still works (" + version_selected + ")");
        }
    }
    ui->imageBox->setCurrentText(version_selected);
}

void ImageMounter::ShowDialog(bool closeAction)
{
    m_isCloseAction = closeAction;
    ui->logField->clear();
    RefreshUI();
    show();
}

void ImageMounter::DownloadImage(DOWNLOAD_TYPE downloadtype)
{
    QString selected_version = ui->imageBox->currentText();
    QString repo_owner = ui->repoBox->currentText();
    m_downloadtype = downloadtype;

    if (m_downloadState == DOWNLOAD_STATE::IDLE)
    {
        if (downloadtype == DOWNLOAD_TYPE::ARCHIVED_IMAGE)
            m_downloadState = DOWNLOAD_STATE::DOWNLOAD;
        else if (downloadtype == DOWNLOAD_TYPE::DIRECT_FILES)
            m_downloadState = DOWNLOAD_STATE::FETCH;
    }

    switch (m_downloadState)
    {
    case DOWNLOAD_STATE::FETCH:
        ui->logField->append("Get image url from repository...");
        m_request->Get(m_repoJson[repo_owner].toObject()["images"].toObject()[selected_version].toString(), [this, repo_owner, selected_version](QNetworkReply::NetworkError responseCode, QJsonDocument ResponseData){
            if (responseCode == QNetworkReply::NetworkError::NoError)
            {
                foreach (const auto& file_json, ResponseData.array())
                {
                    QString target = GetDirectory(DIRECTORY_TYPE::DISKIMAGES) + "/" + repo_owner + "/" + ParseVersion(selected_version) + "/" + file_json.toObject()["name"].toString();
                    m_downloadUrls[target] = file_json.toObject()["download_url"].toString();
                }
                ChangeDownloadState(DOWNLOAD_STATE::DOWNLOAD);
            }
        });
        break;

    case DOWNLOAD_STATE::DOWNLOAD:
    {
        if (downloadtype == DOWNLOAD_TYPE::ARCHIVED_IMAGE)
        {
            m_downloadout = GetDirectory(DIRECTORY_TYPE::DISKIMAGES) + "/" + repo_owner + "/";
            m_downloadurl = m_repoJson[repo_owner].toObject()["images"].toObject()[ui->imageBox->currentText()].toString();
        }
        else if (downloadtype == DOWNLOAD_TYPE::DIRECT_FILES)
        {
            foreach (const auto& location, m_downloadUrls.keys())
            {
                if (!QFileInfo::exists(location))
                {
                    m_downloadout = location;
                    m_downloadurl = m_downloadUrls[m_downloadout];
                    m_downloadUrls.remove(location);
                    break;
                }
                m_downloadUrls.remove(location);
            }
        }
        ui->logField->append("Download " + (m_downloadout.endsWith(".dmg") ? QString("image") : "signature") + " file from repository...");
        m_request->Download(m_downloadurl);
        break;
    }
    case DOWNLOAD_STATE::DONE:
        m_downloadState = DOWNLOAD_STATE::IDLE;
        m_downloadUrls.clear();
        OnDownloadMountClicked();
        break;

    default:
        break;
    }
}

void ImageMounter::ChangeDownloadState(DOWNLOAD_STATE downloadState)
{
    m_downloadState = downloadState;
    DownloadImage(m_downloadtype);
}

void ImageMounter::OnImageClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Choose Image File");
    ui->imageEdit->setText(filename);
}

void ImageMounter::OnSignatureClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Choose Signature File");
    ui->signatureEdit->setText(filename);
}

void ImageMounter::OnMountClicked()
{
    DeviceBridge::Get()->MountImage(ui->imageEdit->text(), ui->signatureEdit->text());
    RefreshUI(false);
}

void ImageMounter::OnDownloadMountClicked()
{
    auto mounted = DeviceBridge::Get()->GetMountedImages();
    if (mounted.length() > 0)
    {
        QMessageBox::information(this, "Disk Mounted!", "Developer disk image mounted", QMessageBox::Ok);
        return;
    }

    if (ui->imageBox->currentText().isEmpty())
        return;

    QString repo_owner = ui->repoBox->currentText();
    bool repo_archived = m_repoJson[repo_owner].toObject()["is_archived"].toBool();

    QStringList diskImages = FindFiles(GetDirectory(DIRECTORY_TYPE::DISKIMAGES) + "/" + repo_owner + "/", QStringList() << "*.dmg" << "*.signature");
    if (FilterVersion(diskImages, ui->imageBox->currentText()) && diskImages.length() == 2)
    {
        QString image_path = diskImages.filter(QRegularExpression(".dmg$")).at(0);
        QString signature_path = diskImages.filter(QRegularExpression(".signature$")).at(0);
        DeviceBridge::Get()->MountImage(image_path, signature_path);
        RefreshUI(false);
    }
    else
    {
        DownloadImage(repo_archived ? DOWNLOAD_TYPE::ARCHIVED_IMAGE : DOWNLOAD_TYPE::DIRECT_FILES);
    }
}

void ImageMounter::OnMounterStatusChanged(QString messages)
{
    ui->logField->append(messages);
}

void ImageMounter::OnDownloadResponse(SimpleRequest::RequestState req_state, int percentage, QNetworkReply::NetworkError error, QByteArray data)
{
    switch (req_state)
    {
    case SimpleRequest::RequestState::STATE_PROGRESS:
        ui->progressBar->setValue(percentage);
        break;

    case SimpleRequest::RequestState::STATE_FINISH:
    {
        if (m_downloadtype == DOWNLOAD_TYPE::ARCHIVED_IMAGE)
        {
            ui->logField->append("Image Package downloaded.");
            QDir().mkpath(GetDirectory(DIRECTORY_TYPE::TEMP));
            QString temp_zip = GetDirectory(DIRECTORY_TYPE::TEMP) + "devimage.temp";
            QSaveFile file(temp_zip);
            file.open(QIODevice::WriteOnly);
            file.write(data.data(), data.size());
            file.commit();

            ui->logField->append("Extract Image Package...");
            struct zip *zf = NULL;
            int errp = 0;
            char errstr[256];
            zf = zip_open(temp_zip.toUtf8().data(), 0, &errp);
            if (!zf) {
                zip_error_to_str(errstr, sizeof(errstr), errp, errno);
                ui->logField->append("ERROR: zip_open: " + QString(errstr) + ": " + QString::number(errp));
                break;
            }
            zip_extract_all(zf, m_downloadout);
            ChangeDownloadState(DOWNLOAD_STATE::DONE);
        }
        else if (m_downloadtype == DOWNLOAD_TYPE::DIRECT_FILES)
        {
            QFileInfo file_info(m_downloadout);
            ui->logField->append((m_downloadout.endsWith(".dmg") ? QString("Image") : "Signature") + " file downloaded.");
            QDir().mkpath(file_info.filePath().remove(file_info.fileName()));
            QSaveFile file(m_downloadout);
            file.open(QIODevice::WriteOnly);
            file.write(data.data(), data.size());
            file.commit();
            ChangeDownloadState(m_downloadUrls.isEmpty() ? DOWNLOAD_STATE::DONE : DOWNLOAD_STATE::DOWNLOAD);
        }
        break;
    }
    case SimpleRequest::RequestState::STATE_ERROR:
        ui->logField->append("Download error: " + QVariant::fromValue(error).toString());
        break;

    default:
        break;
    }
}
