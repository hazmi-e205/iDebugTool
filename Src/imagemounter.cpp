#include "imagemounter.h"
#include "devicebridge.h"
#include <QFileDialog>
#include <QSaveFile>
#include <ui_imagemounter.h>
#include <zip.h>
#include "utils.h"

ImageMounter::ImageMounter(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::ImageMounter)
  , m_request(nullptr)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    connect(ui->imageBtn, SIGNAL(pressed()), this, SLOT(OnImageClicked()));
    connect(ui->signatureBtn, SIGNAL(pressed()), this, SLOT(OnSignatureClicked()));
    connect(ui->mountBtn, SIGNAL(pressed()), this, SLOT(OnMountClicked()));
    connect(ui->mount2Btn, SIGNAL(pressed()), this, SLOT(OnDownloadMountClicked()));
    connect(DeviceBridge::Get(), SIGNAL(MounterStatusChanged(QString)), this, SLOT(OnMounterStatusChanged(QString)));
    m_request = new SimpleRequest();
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
        ui->Signatures->setText(mounted.at(0));
    else
        ui->Signatures->setText("No image mounted!");

    if (IsInternetOn())
    {
        ui->onlineTab->show();
        if (fetchImages)
        {
            ui->repoEdit->setText("https://github.com/haikieu/xcode-developer-disk-image-all-platforms");
            ui->logField->append("Fetch image list...");
            m_request->Get("https://api.github.com/repos/haikieu/xcode-developer-disk-image-all-platforms/contents/DiskImages/iPhoneOS.platform/DeviceSupport?ref=master", [this, OSVersion](QNetworkReply::NetworkError responseCode, QJsonDocument ResponseData){
                if (responseCode == QNetworkReply::NetworkError::NoError)
                {
                    ui->logField->append("Parsing data from repository...");
                    auto rawdata = ResponseData.array();
                    for (int idx = 0; idx < rawdata.count(); ++idx)
                    {
                        QString nameStr = rawdata.at(idx)["name"].toString();
                        m_imagelinks[VersionToUInt(nameStr)] = rawdata.at(idx)["download_url"].toString();
                    }

                    QString version_selected;
                    foreach (const quint64 &image_ver, m_imagelinks.keys())
                    {
                        quint64 os_version = VersionToUInt(OSVersion);
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
                    ui->logField->append("Image list parsed.");
                }
            });
        }
    }
    else
    {
        ui->offlineTab->show();
    }
}

void ImageMounter::ShowDialog()
{
    ui->logField->clear();
    RefreshUI();
    show();
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
    QStringList diskImages = FindFiles(GetDirectory(DIRECTORY_TYPE::DISKIMAGES), QStringList() << "*.dmg" << "*.signature");
    if (FilterVersion(diskImages, ui->imageBox->currentText()))
    {
        QString image_path = diskImages.filter(QRegularExpression(".dmg$")).at(0);
        QString signature_path = diskImages.filter(QRegularExpression(".signature$")).at(0);
        DeviceBridge::Get()->MountImage(image_path, signature_path);
        RefreshUI(false);
    }
    else
    {
        ui->logField->append("Download image from repository...");
        m_request->Download(m_imagelinks[VersionToUInt(ui->imageBox->currentText())], [this](SimpleRequest::RequestState state, int percentage, QNetworkReply::NetworkError error, QByteArray data){
            QString temp_zip = GetDirectory(DIRECTORY_TYPE::TEMP) + "devimage.temp";
            QDir().mkpath(GetDirectory(DIRECTORY_TYPE::TEMP));
            switch (state)
            {
            case SimpleRequest::RequestState::STATE_PROGRESS:
                ui->progressBar->setValue(percentage);
                break;

            case SimpleRequest::RequestState::STATE_FINISH:
            {
                ui->logField->append("Image Package downloaded.");
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
                zip_extract_all(zf, GetDirectory(DIRECTORY_TYPE::DISKIMAGES));
                OnDownloadMountClicked();
                break;
            }
            case SimpleRequest::RequestState::STATE_ERROR:
                ui->logField->append("Download error: " + QVariant::fromValue(error).toString());
                break;

            default:
                break;
            }
        });
    }
}

void ImageMounter::OnMounterStatusChanged(QString messages)
{
    ui->logField->append(messages);
}
