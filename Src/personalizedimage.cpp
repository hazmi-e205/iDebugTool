#include "personalizedimage.h"

#include "utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstdlib>
#include <plist/plist.h>

PersonalizedImage::PersonalizedImage()
    : m_imageDir(GetDirectory(DIRECTORY_TYPE::DISKIMAGES) + "/Personalized")
    , m_manifestPath(QDir(m_imageDir).filePath(MANIFEST_NAME))
{
}

bool PersonalizedImage::ShouldUsePersonalizedImage(const QString& productVersion) const
{
    QStringList parts = productVersion.split(".");
    int majorVersion = parts.isEmpty() ? 0 : parts[0].toInt();
    return majorVersion >= 17;
}

QString PersonalizedImage::ImageDir() const
{
    return m_imageDir;
}

QString PersonalizedImage::ManifestPath() const
{
    return m_manifestPath;
}

QString PersonalizedImage::RepoOwner() const
{
    return "doronz88";
}

QString PersonalizedImage::UiImageVersion() const
{
    return "17.0";
}

QString PersonalizedImage::UiHint() const
{
    return "Personalized image is used for iOS 17 and higher";
}

bool PersonalizedImage::HasReadyImage(const QString& productType, bool& productTypeFound) const
{
    productTypeFound = true;
    if (!QFileInfo::exists(m_manifestPath))
        return false;

    QString personalizedDmgPath;
    QString trustCachePath;
    QStringList supportedProductTypes;
    if (!ParseManifest(personalizedDmgPath, trustCachePath, supportedProductTypes))
        return false;

    if (!supportedProductTypes.isEmpty() && !ProductTypeInSupportedList(productType, supportedProductTypes))
    {
        productTypeFound = false;
        return false;
    }

    const QString localDmgPath = BuildLocalPath(personalizedDmgPath);
    const QString localTrustCachePath = BuildLocalPath(trustCachePath);
    if (!QFileInfo::exists(localDmgPath) || !IsDownloadedArtifactValid(localDmgPath))
        return false;
    if (!QFileInfo::exists(localTrustCachePath) || !IsDownloadedArtifactValid(localTrustCachePath))
        return false;

    return true;
}

PersonalizedImage::PrepareResult PersonalizedImage::PrepareDownloads(const QString& productType, bool forceDownload, QMap<QString, QString>& downloadUrls, QString& errorMessage) const
{
    errorMessage.clear();
    downloadUrls.clear();
    QDir().mkpath(m_imageDir);

    if (forceDownload || !QFileInfo::exists(m_manifestPath))
        downloadUrls[m_manifestPath] = BuildRemoteUrl(MANIFEST_NAME);

    if (!downloadUrls.isEmpty())
        return PrepareResult::OK;

    QString personalizedDmgPath;
    QString trustCachePath;
    QStringList supportedProductTypes;
    if (!ParseManifest(personalizedDmgPath, trustCachePath, supportedProductTypes))
    {
        errorMessage = "Error: Failed to parse personalized BuildManifest.plist.";
        return PrepareResult::ERROR;
    }

    if (!supportedProductTypes.isEmpty() && !ProductTypeInSupportedList(productType, supportedProductTypes))
    {
        if (!forceDownload)
            return PrepareResult::RETRY_WITH_FORCE_DOWNLOAD;
        errorMessage = "Error: image for the model not found";
        return PrepareResult::MODEL_NOT_FOUND;
    }

    const QString localDmgPath = BuildLocalPath(personalizedDmgPath);
    const QString localTrustCachePath = BuildLocalPath(trustCachePath);
    if (forceDownload || !QFileInfo::exists(localDmgPath) || !IsDownloadedArtifactValid(localDmgPath))
        downloadUrls[localDmgPath] = BuildRemoteUrl(IMAGE_NAME);
    if (forceDownload || !QFileInfo::exists(localTrustCachePath) || !IsDownloadedArtifactValid(localTrustCachePath))
        downloadUrls[localTrustCachePath] = BuildRemoteUrl(TRUSTCACHE_NAME);

    return PrepareResult::OK;
}

bool PersonalizedImage::ParseManifest(QString& personalizedDmgPath, QString& trustCachePath, QStringList& supportedProductTypes) const
{
    personalizedDmgPath.clear();
    trustCachePath.clear();
    supportedProductTypes.clear();

    QByteArray manifestPathUtf8 = m_manifestPath.toUtf8();
    plist_t manifest = nullptr;
    if (plist_read_from_file(manifestPathUtf8.constData(), &manifest, NULL) != 0 || !manifest)
        return false;

    plist_t supported = plist_dict_get_item(manifest, "SupportedProductTypes");
    if (supported && plist_get_node_type(supported) == PLIST_ARRAY)
    {
        const uint32_t count = plist_array_get_size(supported);
        for (uint32_t idx = 0; idx < count; ++idx)
        {
            plist_t item = plist_array_get_item(supported, idx);
            if (!item || plist_get_node_type(item) != PLIST_STRING)
                continue;
            char* productType = nullptr;
            plist_get_string_val(item, &productType);
            if (productType && productType[0] != '\0')
                supportedProductTypes.append(QString::fromUtf8(productType));
            if (productType)
                free(productType);
        }
    }

    plist_t buildIdentities = plist_dict_get_item(manifest, "BuildIdentities");
    if (!buildIdentities || plist_get_node_type(buildIdentities) != PLIST_ARRAY || plist_array_get_size(buildIdentities) == 0)
    {
        plist_free(manifest);
        return false;
    }

    plist_t firstIdentity = plist_array_get_item(buildIdentities, 0);
    plist_t dmgPathNode = plist_access_path(firstIdentity, 4, "Manifest", "PersonalizedDMG", "Info", "Path");
    plist_t trustCachePathNode = plist_access_path(firstIdentity, 4, "Manifest", "LoadableTrustCache", "Info", "Path");
    if (!dmgPathNode || !trustCachePathNode)
    {
        plist_free(manifest);
        return false;
    }

    const char* dmgPath = plist_get_string_ptr(dmgPathNode, NULL);
    const char* tcPath = plist_get_string_ptr(trustCachePathNode, NULL);
    if (!dmgPath || !tcPath)
    {
        plist_free(manifest);
        return false;
    }

    personalizedDmgPath = QString::fromUtf8(dmgPath);
    trustCachePath = QString::fromUtf8(tcPath);
    plist_free(manifest);
    return !personalizedDmgPath.isEmpty() && !trustCachePath.isEmpty();
}

bool PersonalizedImage::ProductTypeInSupportedList(const QString& productType, const QStringList& supportedProductTypes) const
{
    for (const QString& supportedType : supportedProductTypes)
    {
        if (supportedType.compare(productType, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QString PersonalizedImage::NormalizeRelativePath(const QString& relativePath) const
{
    QString normalized = relativePath;
    normalized.replace("\\", "/");
    while (normalized.startsWith("/"))
        normalized.remove(0, 1);
    return normalized;
}

QString PersonalizedImage::BuildRemoteUrl(const QString& relativePath) const
{
    return QString(RAW_BASE) + NormalizeRelativePath(relativePath);
}

QString PersonalizedImage::BuildLocalPath(const QString& relativePath) const
{
    return QDir(m_imageDir).filePath(NormalizeRelativePath(relativePath));
}

bool PersonalizedImage::IsDownloadedArtifactValid(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray head = file.read(256).trimmed();
    if (head.isEmpty())
        return false;

    if (head.startsWith("404: Not Found"))
        return false;
    if (head.startsWith("<!DOCTYPE html") || head.startsWith("<html"))
        return false;

    return true;
}
