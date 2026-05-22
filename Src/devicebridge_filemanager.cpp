#include "devicebridge.h"
#include <QDir>
#include <QFileInfo>
#include <cstring>


void DeviceBridge::GetAccessibleStorage(QString startPath, QString bundleId, bool partialUpdate)
{
    afc_filemanager_action(MobileOperation::FILE_LIST, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        QString normalizedPath = startPath.trimmed();
        if (normalizedPath.isEmpty())
            normalizedPath = "/";
        if (normalizedPath.length() > 1 && normalizedPath.endsWith('/'))
            normalizedPath.chop(1);

        if (partialUpdate && normalizedPath != "/") {
            QString prefix = normalizedPath + "/";
            for (auto it = m_accessibleStorage.begin(); it != m_accessibleStorage.end(); ) {
                if (it.key().startsWith(prefix))
                    it = m_accessibleStorage.erase(it);
                else
                    ++it;
            }
        } else {
            m_accessibleStorage.clear();
        }
        emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::FETCH, 0, bundleId);
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        int total_items = 0;
        char **entries = NULL;
        if (afc_read_directory(afc, normalizedPath.toStdString().c_str(), &entries) == AFC_E_SUCCESS && entries) {
            for (int i = 0; entries[i]; ++i) {
                if (strcmp(entries[i], ".") == 0 || strcmp(entries[i], "..") == 0) continue;
                total_items++;
            }
            afc_dictionary_free(entries);
        }

        int visited_items = 0;
        int direct_dir_count = 0;
        int direct_file_count = 0;
        int last_percentage = -1;
        auto progress_cb = [&](int current, int total) {
            if (total <= 0) return;
            int percentage = int((float(current) / float(total)) * 100.f);
            if (percentage != last_percentage) {
                last_percentage = percentage;
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::FETCH, percentage, bundleId);
            }
        };
        afc_list_directory_shallow(afc, normalizedPath.toStdString().c_str(), &visited_items, total_items, progress_cb, should_stop, &direct_dir_count, &direct_file_count);

        if (normalizedPath != "/") {
            auto existing = m_accessibleStorage.value(normalizedPath);
            existing.isDirectory = true;
            existing.childDirectoryCount = direct_dir_count;
            existing.childFileCount = direct_file_count;
            m_accessibleStorage[normalizedPath] = existing;
        }

        if (should_stop()) {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::FETCH, last_percentage < 0 ? 0 : last_percentage, bundleId);
            return;
        }

        emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::FETCH, 100, bundleId);
        emit AccessibleStorageReceived(m_accessibleStorage, normalizedPath, partialUpdate);
    }, bundleId);
}

void DeviceBridge::PushToStorage(QString localPath, QString devicePath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PUSH_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        int percentage = 0;
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        auto callback = [&](uint32_t uploaded_bytes, uint32_t total_bytes)
        {
            percentage = int((float(uploaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PUSH, percentage, devicePath);
        };
        int result = afc_upload_file(afc, localPath, devicePath, callback, should_stop);
        if (result == -3 || should_stop()) {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PUSH, percentage, "Cancelled");
            return;
        }
        emit FileManagerChanged(result == 0 ? GenericStatus::SUCCESS : GenericStatus::FAILED, FileOperation::PUSH, percentage, devicePath);
    }, bundleId);
}

void DeviceBridge::PushMultipleToStorage(QStringList localPaths, QString deviceFolderPath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PUSH_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        // Pre-calculate total size of all files
        int64_t totalBytes = 0;
        for (const QString& path : localPaths)
            totalBytes += QFileInfo(path).size();
        if (totalBytes <= 0) totalBytes = 1;

        int totalFiles = localPaths.size();
        int64_t bytesDoneTotal = 0;
        QString lastDevicePath = deviceFolderPath;
        bool hasError = false;
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };

        for (int i = 0; i < totalFiles; i++) {
            if (should_stop()) {
                emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PUSH,
                    (int)((float)bytesDoneTotal / (float)totalBytes * 100.f), "Cancelled");
                return;
            }

            const QString& localPath = localPaths[i];
            QFileInfo fi(localPath);
            QString filename = fi.fileName();
            QString destPath = deviceFolderPath + filename;
            lastDevicePath = destPath;

            QString fileLabel = QString("[%1/%2] %3").arg(i + 1).arg(totalFiles).arg(filename);
            int64_t fileStartBytes = bytesDoneTotal;

            auto callback = [&, fileLabel, fileStartBytes](uint32_t uploaded_bytes, uint32_t /*total_bytes*/) {
                int64_t progress = fileStartBytes + (int64_t)uploaded_bytes;
                int percentage = (int)((float)progress / (float)totalBytes * 100.f);
                percentage = std::min(percentage, 99);
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PUSH, percentage, fileLabel);
            };

            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PUSH,
                (int)((float)bytesDoneTotal / (float)totalBytes * 100.f), fileLabel);

            int result = afc_upload_file(afc, localPath, destPath, callback, should_stop);
            bytesDoneTotal += fi.size();

            if (result == -3 || should_stop()) {
                emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PUSH,
                    (int)((float)bytesDoneTotal / (float)totalBytes * 100.f), "Cancelled");
                return;
            }

            if (result != 0) {
                hasError = true;
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PUSH,
                    (int)((float)bytesDoneTotal / (float)totalBytes * 100.f),
                    fileLabel + " - FAILED");
            }
        }

        if (hasError)
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PUSH, 100, "Completed with errors");
        else
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::PUSH, 100, lastDevicePath);
    }, bundleId);
}

void DeviceBridge::PullFromStorage(QString devicePath, QString localPath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PULL_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        int percentage = 0;
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };
        auto callback = [&](uint32_t downloaded_bytes, uint32_t total_bytes)
        {
            if (total_bytes > 0) {
                percentage = int((float(downloaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
            }
            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL, percentage, devicePath);
        };
        int result = afc_download_file(afc, devicePath, localPath, callback, should_stop);
        if (result == -3 || should_stop()) {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL, percentage, "Cancelled");
            return;
        }
        if (result != 0) {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL, percentage, devicePath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to pull file: " + devicePath + "! " + QString::number(result));
            return;
        }
        emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::PULL, percentage, devicePath);
    }, bundleId);
}

void DeviceBridge::PullMultipleFromStorage(QList<QPair<QString,QString>> pairs, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PULL_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool> cancel_flag){
        int totalFiles = pairs.size();
        if (totalFiles == 0) return;

        bool hasError = false;
        auto should_stop = [cancel_flag]() { return cancel_flag && cancel_flag->load(); };

        for (int i = 0; i < totalFiles; i++) {
            if (should_stop()) {
                emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL,
                    (i * 100) / totalFiles, "Cancelled");
                return;
            }

            const QString& devicePath = pairs[i].first;
            const QString& localPath  = pairs[i].second;
            QString filename = QFileInfo(devicePath).fileName();

            QString fileLabel = QString("[%1/%2] %3").arg(i + 1).arg(totalFiles).arg(filename);

            // Ensure local directory exists
            QDir().mkpath(QFileInfo(localPath).dir().absolutePath());

            int filesDone = i;
            auto callback = [&, fileLabel, filesDone](uint32_t downloaded_bytes, uint32_t total_bytes) {
                int currentFilePct = (total_bytes > 0)
                    ? (int)((float)downloaded_bytes / (float)total_bytes * 100.f)
                    : 0;
                int percentage = std::min((filesDone * 100 + currentFilePct) / totalFiles, 99);
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL, percentage, fileLabel);
            };

            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL,
                (i * 100) / totalFiles, fileLabel);

            int result = afc_download_file(afc, devicePath, localPath, callback, should_stop);

            if (result == -3 || should_stop()) {
                emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL,
                    ((i + 1) * 100) / totalFiles, "Cancelled");
                return;
            }

            if (result != 0) {
                hasError = true;
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL,
                    ((i + 1) * 100) / totalFiles, fileLabel + " - FAILED - " + QString::number(result));
            }
        }

        if (hasError)
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL, 100, "Completed with errors");
        else
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::PULL, 100, pairs.last().first);
    }, bundleId);
}

void DeviceBridge::DeleteMultipleFromStorage(QStringList devicePaths, QString bundleId)
{
    afc_filemanager_action(MobileOperation::DELETE_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool>){
        int totalFiles = devicePaths.size();
        if (totalFiles == 0) return;

        bool hasError = false;

        for (int i = 0; i < totalFiles; i++) {
            const QString& devicePath = devicePaths[i];
            QString filename = QFileInfo(devicePath).fileName();
            QString fileLabel = QString("[%1/%2] %3").arg(i + 1).arg(totalFiles).arg(filename);

            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::DELETE_OP,
                (i * 100) / totalFiles, fileLabel);

            afc_error_t err = afc_remove_path(afc, devicePath.toUtf8().data());
            if (err != AFC_E_SUCCESS) {
                hasError = true;
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::DELETE_OP,
                    ((i + 1) * 100) / totalFiles, fileLabel + " - FAILED - " + QString::number(err));
            }
        }

        // Find common ancestor directory of all deleted paths for partial refresh
        auto splitPath = [](const QString& p) { return p.split('/', Qt::SkipEmptyParts); };
        QStringList common = splitPath(QFileInfo(devicePaths.first()).path());
        for (const QString& path : devicePaths) {
            QStringList parts = splitPath(QFileInfo(path).path());
            int minLen = std::min(common.size(), parts.size());
            int i = 0;
            while (i < minLen && common[i] == parts[i]) i++;
            common = common.mid(0, i);
        }
        QString commonDir = (common.isEmpty() ? "/" : "/" + common.join('/')) + "/";

        if (hasError)
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::DELETE_OP, 100, "Completed with errors");
        else
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::DELETE_OP, 100, commonDir);
    }, bundleId);
}

void DeviceBridge::DeleteFromStorage(QString devicePath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::DELETE_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool>){
        afc_error_t err = afc_remove_path(afc, devicePath.toUtf8().data());
        if (err == AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::DELETE_OP, 100, devicePath);
        } else {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::DELETE_OP, 100, devicePath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to delete: " + devicePath + "! " + QString::number(err));
        }
    }, bundleId);
}

void DeviceBridge::MakeDirectoryToStorage(QString devicePath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::NEW_FOLDER, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool>){
        afc_error_t err = afc_make_directory(afc, devicePath.toUtf8().data());
        if (err == AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::MAKE_FOLDER, 100, devicePath);
        } else {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::MAKE_FOLDER, 100, devicePath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to make folder: " + devicePath + "! " + QString::number(err));
        }
    }, bundleId);
}

void DeviceBridge::RenameToStorage(QString oldPath, QString newPath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::RENAME_FILE, [=, this](afc_client_t& afc, std::shared_ptr<DeviceClient>, std::shared_ptr<std::atomic_bool>){
        afc_error_t err = afc_rename_path(afc, oldPath.toUtf8().data(), newPath.toUtf8().data());
        if (err == AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::RENAME, 100, newPath);
        } else {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::RENAME, 100, newPath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to rename: " + oldPath + " to " + newPath + "! " + QString::number(err));
        }
    }, bundleId);
}

void DeviceBridge::afc_filemanager_action(MobileOperation op, std::function<void(afc_client_t &afc, std::shared_ptr<DeviceClient> client, std::shared_ptr<std::atomic_bool> cancel_flag)> action, const QString& bundleId)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        if (!bundleId.isEmpty())
        {
            QStringList serviceIds = QStringList() << HOUSE_ARREST_SERVICE_NAME;
            if (!CreateClient(op, serviceIds))
                return;

            auto client = m_clients.value(op);
            if (!client)
                return;
            auto cancel_flag = m_cancelFlags.value(op);
            if (!cancel_flag)
                return;

            lockdownd_service_descriptor_t service = GetService(op, serviceIds);
            if (!service)
                return;

            house_arrest_error_t err = house_arrest_client_new(client->device, service, &client->house_arrest);
            if (err != HOUSE_ARREST_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));

            err = house_arrest_send_command(client->house_arrest, "VendContainer", bundleId.toUtf8().data());
            if (err != HOUSE_ARREST_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Access Denied to " + bundleId + "'s VendContainer client! " + QString::number(err));

            afc_error_t aerr = afc_client_new_from_house_arrest_client(client->house_arrest, &client->afc);
            if (aerr != AFC_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to afc with " + serviceIds.join(", ") + " client! " + QString::number(aerr));

            //call function pass from params
            action(client->afc, client, cancel_flag);
        }
        else
        {
            QStringList serviceIds = QStringList() << AFC_SERVICE_NAME;
            if (!CreateClient(op, serviceIds))
                return;

            auto client = m_clients.value(op);
            if (!client)
                return;
            auto cancel_flag = m_cancelFlags.value(op);
            if (!cancel_flag)
                return;

            lockdownd_service_descriptor_t service = GetService(op, serviceIds);
            if (!service)
                return;

            afc_error_t aerr = afc_client_new(client->device, service, &client->afc);
            if (aerr != AFC_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(aerr));

            //call function pass from params
            action(client->afc, client, cancel_flag);
        }
        //clear instances
        RemoveClient(op);
    });
}
