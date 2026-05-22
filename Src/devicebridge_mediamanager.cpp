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

bool AfcGetFileInfo(afc_client_t afc, const char* path, bool* outIsDir, quint64* outSize, qint64* outModTimeNs = nullptr)
{
    char** info = nullptr;
    if (afc_get_file_info(afc, path, &info) != AFC_E_SUCCESS || !info)
        return false;

    bool isDir = false;
    quint64 size = 0;
    qint64 mtime = 0;
    for (int j = 0; info[j]; j += 2) {
        if (std::strcmp(info[j], "st_ifmt") == 0 && std::strcmp(info[j + 1], "S_IFDIR") == 0)
            isDir = true;
        else if (std::strcmp(info[j], "st_size") == 0) {
            try { size = std::stoull(info[j + 1]); } catch (...) { size = 0; }
        }
        else if (std::strcmp(info[j], "st_mtime") == 0) {
            try { mtime = static_cast<qint64>(std::stoll(info[j + 1])); } catch (...) { mtime = 0; }
        }
    }
    afc_dictionary_free(info);
    if (outIsDir) *outIsDir = isDir;
    if (outSize) *outSize = size;
    if (outModTimeNs) *outModTimeNs = mtime;
    return true;
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

        QString fullPath = (path == "/") ? (QString("/") + name) : (path + "/" + name);
        bool isDir = false;
        quint64 size = 0;
        qint64 mtime = 0;
        if (!AfcGetFileInfo(afc, fullPath.toUtf8().constData(), &isDir, &size, &mtime))
            continue;

        if (isDir) {
            WalkForMedia(afc, fullPath, out, should_stop);
        } else {
            bool isVideo = false;
            if (IsMediaExtension(QString::fromUtf8(name), &isVideo)) {
                DeviceBridge::MediaEntry entry;
                entry.devicePath = fullPath;
                entry.sizeInBytes = size;
                entry.modTimeNs = mtime;
                entry.isVideo = isVideo;
                out.append(entry);
            }
        }
    }
    afc_dictionary_free(list);
}
} // namespace

void DeviceBridge::EnumerateMediaFiles(QString rootPath)
{
    afc_filemanager_action(MobileOperation::FILE_LIST, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        QString normalized = rootPath.trimmed();
        if (normalized.isEmpty()) normalized = "/";
        if (normalized.length() > 1 && normalized.endsWith('/'))
            normalized.chop(1);

        QList<MediaEntry> entries;
        WalkForMedia(afc, normalized, entries, should_stop);

        if (should_stop()) {
            emit MediaFilesEnumerated(QList<MediaEntry>());
            return;
        }

        // Direct path construction: /PhotoData/Thumbnails/V2 + originalPath + /5005.JPG.
        const QString thumbRoot = "/PhotoData/Thumbnails/V2";
        for (MediaEntry& e : entries)
            e.thumbDevicePath = thumbRoot + e.devicePath + "/5005.JPG";

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
            int result = afc_download_file(afc, devicePath, localPath, nullptr, should_stop);
            bool success = (result == 0);
            if (!success)
                QFile::remove(localPath);
            emit MediaFileFetched(devicePath, success ? localPath : QString(), success);
        }
    });
}
