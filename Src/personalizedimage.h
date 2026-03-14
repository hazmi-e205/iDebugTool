#ifndef PERSONALIZEDIMAGE_H
#define PERSONALIZEDIMAGE_H

#include <QMap>
#include <QString>
#include <QStringList>

class PersonalizedImage
{
public:
    enum class PrepareResult
    {
        OK,
        RETRY_WITH_FORCE_DOWNLOAD,
        MODEL_NOT_FOUND,
        ERROR
    };

    PersonalizedImage();

    bool ShouldUsePersonalizedImage(const QString& productVersion) const;
    QString ImageDir() const;
    QString ManifestPath() const;
    QString RepoOwner() const;
    QString UiImageVersion() const;
    QString UiHint() const;

    bool HasReadyImage(const QString& productType, bool& productTypeFound) const;
    PrepareResult PrepareDownloads(const QString& productType, bool forceDownload, QMap<QString, QString>& downloadUrls, QString& errorMessage) const;

private:
    static constexpr const char* RAW_BASE = "https://raw.githubusercontent.com/doronz88/DeveloperDiskImage/main/PersonalizedImages/Xcode_iOS_DDI_Personalized/";
    static constexpr const char* MANIFEST_NAME = "BuildManifest.plist";
    static constexpr const char* IMAGE_NAME = "Image.dmg";
    static constexpr const char* TRUSTCACHE_NAME = "Image.dmg.trustcache";

    QString m_imageDir;
    QString m_manifestPath;

    bool ParseManifest(QString& personalizedDmgPath, QString& trustCachePath, QStringList& supportedProductTypes) const;
    bool ProductTypeInSupportedList(const QString& productType, const QStringList& supportedProductTypes) const;
    QString NormalizeRelativePath(const QString& relativePath) const;
    QString BuildRemoteUrl(const QString& relativePath) const;
    QString BuildLocalPath(const QString& relativePath) const;
    bool IsDownloadedArtifactValid(const QString& filePath) const;
};

#endif // PERSONALIZEDIMAGE_H
