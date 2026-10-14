#include "devicebridge.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstring>

namespace {
bool IsMediaExtension(const QString& filename, bool* outIsVideo)
{
    static const QSet<QString> imageExts = {
        "jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff", "webp", "heic", "heif"
    };
    static const QSet<QString> videoExts = {
        "mov", "mp4", "m4v", "avi", "mkv", "wmv", "3gp", "3g2"
    };
    const QString ext = QFileInfo(filename).suffix().toLower();
    if (imageExts.contains(ext)) {
        if (outIsVideo) *outIsVideo = false;
        return true;
    }
    if (videoExts.contains(ext)) {
        if (outIsVideo) *outIsVideo = true;
        return true;
    }
    return false;
}

struct AfcFileInfo {
    bool ok = false;
    bool isDir = false;
    quint64 size = 0;
    qint64 modTimeNs = 0;
};

AfcFileInfo AfcGetFileInfo(afc_client_t afc, const char* path)
{
    AfcFileInfo result;
    char** info = nullptr;
    if (afc_get_file_info(afc, path, &info) != AFC_E_SUCCESS || !info)
        return result;

    for (int j = 0; info[j]; j += 2) {
        if (std::strcmp(info[j], "st_ifmt") == 0 && std::strcmp(info[j + 1], "S_IFDIR") == 0)
            result.isDir = true;
        else if (std::strcmp(info[j], "st_size") == 0) {
            try { result.size = std::stoull(info[j + 1]); } catch (...) {}
        }
        else if (std::strcmp(info[j], "st_mtime") == 0) {
            try { result.modTimeNs = static_cast<qint64>(std::stoll(info[j + 1])); } catch (...) {}
        }
    }
    afc_dictionary_free(info);
    result.ok = true;
    return result;
}

void WalkForMedia(afc_client_t afc, const QString& path, QList<DeviceBridge::MediaEntry>& out, std::function<bool()> should_stop)
{
    if (should_stop && should_stop())
        return;
    char** list = nullptr;
    if (afc_read_directory(afc, path.toUtf8().constData(), &list) != AFC_E_SUCCESS || !list)
        return;

    for (int i = 0; list[i]; ++i) {
        if (should_stop && should_stop()) break;
        const char* name = list[i];
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) continue;

        const QString fullPath = path + "/" + name;
        const AfcFileInfo info = AfcGetFileInfo(afc, fullPath.toUtf8().constData());
        if (!info.ok) continue;

        if (info.isDir) {
            WalkForMedia(afc, fullPath, out, should_stop);
            continue;
        }

        bool isVideo = false;
        if (!IsMediaExtension(QString::fromUtf8(name), &isVideo))
            continue;

        DeviceBridge::MediaEntry entry;
        entry.devicePath = fullPath;
        entry.sizeInBytes = info.size;
        entry.modTimeNs = info.modTimeNs;
        entry.isVideo = isVideo;
        out.append(entry);
    }
    afc_dictionary_free(list);
}

QString FindThumbnailPath(afc_client_t afc, const QString& devicePath)
{
    const QString thumbDir = "/PhotoData/Thumbnails/V2" + devicePath;
    char** list = nullptr;
    if (afc_read_directory(afc, thumbDir.toUtf8().constData(), &list) != AFC_E_SUCCESS || !list)
        return QString();

    QString result;
    for (int i = 0; list[i] && result.isEmpty(); ++i) {
        const QString name = QString::fromUtf8(list[i]);
        if (name.endsWith(".jpg", Qt::CaseInsensitive))
            result = thumbDir + "/" + name;
    }
    afc_dictionary_free(list);
    return result;
}
} // namespace

void DeviceBridge::EnumerateMediaFiles()
{
    afc_filemanager_action(MobileOperation::FILE_LIST, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };

        QList<MediaEntry> entries;
        WalkForMedia(afc, "/DCIM", entries, should_stop);

        if (should_stop()) {
            emit MediaFilesEnumerated({});
            return;
        }

        // Thumbnails live under /PhotoData/Thumbnails/V2 + originalPath; the file name varies by iOS version.
        for (MediaEntry& e : entries) {
            if (should_stop()) {
                emit MediaFilesEnumerated({});
                return;
            }
            e.thumbDevicePath = FindThumbnailPath(afc, e.devicePath);
        }

        emit MediaFilesEnumerated(entries);
    });
}

void DeviceBridge::FetchMediaFiles(QList<QPair<QString,QString>> requests)
{
    if (requests.isEmpty())
        return;
    afc_filemanager_action(MobileOperation::PREVIEW_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        for (const auto& req : requests) {
            if (should_stop()) break;
            const QString& devicePath = req.first;
            const QString& localPath = req.second;
            QDir().mkpath(QFileInfo(localPath).absolutePath());
            const bool success = (afc_download_file(afc, devicePath, localPath, nullptr, should_stop) == 0);
            if (!success)
                QFile::remove(localPath);
            emit MediaFileFetched(devicePath, success ? localPath : QString(), success);
        }
    });
}
