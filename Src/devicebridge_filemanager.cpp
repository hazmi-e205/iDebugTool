#include "devicebridge.h"
#include <QDir>
#include <QFileInfo>


void DeviceBridge::GetAccessibleStorage(QString startPath, QString bundleId, bool partialUpdate)
{
    afc_filemanager_action(MobileOperation::FILE_LIST, [=, this](afc_client_t& afc){
        if (partialUpdate) {
            QString prefix = startPath + "/";
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
        int total_items = afc_count_recursive(afc, startPath.toStdString().c_str());
        int visited_items = 0;
        int last_percentage = -1;
        auto progress_cb = [&](int current, int total) {
            if (total <= 0) return;
            int percentage = int((float(current) / float(total)) * 100.f);
            if (percentage != last_percentage) {
                last_percentage = percentage;
                emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::FETCH, percentage, bundleId);
            }
        };
        afc_traverse_recursive(afc, startPath.toStdString().c_str(), &visited_items, total_items, progress_cb);
        emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::FETCH, 100, bundleId);
        emit AccessibleStorageReceived(m_accessibleStorage);
    }, bundleId);
}

void DeviceBridge::PushToStorage(QString localPath, QString devicePath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PUSH_FILE, [=, this](afc_client_t& afc){
        int percentage = 0;
        auto callback = [&](uint32_t uploaded_bytes, uint32_t total_bytes)
        {
            percentage = int((float(uploaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PUSH, percentage, devicePath);
        };
        int result = afc_upload_file(afc, localPath, devicePath, callback);
        emit FileManagerChanged(result == 0 ? GenericStatus::SUCCESS : GenericStatus::FAILED, FileOperation::PUSH, percentage, devicePath);
    }, bundleId);
}

void DeviceBridge::PushMultipleToStorage(QStringList localPaths, QString deviceFolderPath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::PUSH_FILE, [=, this](afc_client_t& afc){
        // Pre-calculate total size of all files
        int64_t totalBytes = 0;
        for (const QString& path : localPaths)
            totalBytes += QFileInfo(path).size();
        if (totalBytes <= 0) totalBytes = 1;

        int totalFiles = localPaths.size();
        int64_t bytesDoneTotal = 0;
        QString lastDevicePath = deviceFolderPath;
        bool hasError = false;

        for (int i = 0; i < totalFiles; i++) {
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

            int result = afc_upload_file(afc, localPath, destPath, callback);
            bytesDoneTotal += fi.size();

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
    afc_filemanager_action(MobileOperation::PULL_FILE, [=, this](afc_client_t& afc){
        int percentage = 0;
        auto callback = [&](uint32_t downloaded_bytes, uint32_t total_bytes)
        {
            if (total_bytes > 0) {
                percentage = int((float(downloaded_bytes) / (float(total_bytes) * 2.f)) * 100.f);
            }
            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL, percentage, devicePath);
        };
        int result = afc_download_file(afc, devicePath, localPath, callback);
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
    afc_filemanager_action(MobileOperation::PULL_FILE, [=, this](afc_client_t& afc){
        int totalFiles = pairs.size();
        if (totalFiles == 0) return;

        bool hasError = false;

        for (int i = 0; i < totalFiles; i++) {
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

            int result = afc_download_file(afc, devicePath, localPath, callback);

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

void DeviceBridge::DeleteFromStorage(QString devicePath, QString bundleId)
{
    afc_filemanager_action(MobileOperation::DELETE_FILE, [=, this](afc_client_t& afc){
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
    afc_filemanager_action(MobileOperation::NEW_FOLDER, [=, this](afc_client_t& afc){
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
    afc_filemanager_action(MobileOperation::RENAME_FILE, [=, this](afc_client_t& afc){
        afc_error_t err = afc_rename_path(afc, oldPath.toUtf8().data(), newPath.toUtf8().data());
        if (err == AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::RENAME, 100, newPath);
        } else {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::RENAME, 100, newPath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to rename: " + oldPath + " to " + newPath + "! " + QString::number(err));
        }
    }, bundleId);
}

void DeviceBridge::afc_filemanager_action(MobileOperation op, std::function<void(afc_client_t &afc)> action, const QString& bundleId)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        if (!bundleId.isEmpty())
        {
            QStringList serviceIds = QStringList() << HOUSE_ARREST_SERVICE_NAME;
            if (!CreateClient(op, serviceIds))
                return;

            lockdownd_service_descriptor_t service = GetService(op, serviceIds);
            if (!service)
                return;

            house_arrest_error_t err = house_arrest_client_new(m_clients[op]->device, service, &m_clients[op]->house_arrest);
            if (err != HOUSE_ARREST_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(err));

            err = house_arrest_send_command(m_clients[op]->house_arrest, "VendContainer", bundleId.toUtf8().data());
            if (err != HOUSE_ARREST_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Access Denied to " + bundleId + "'s VendContainer client! " + QString::number(err));

            afc_error_t aerr = afc_client_new_from_house_arrest_client(m_clients[op]->house_arrest, &m_clients[op]->afc);
            if (aerr != AFC_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to afc with " + serviceIds.join(", ") + " client! " + QString::number(aerr));

            //call function pass from params
            action(m_clients[op]->afc);
        }
        else
        {
            QStringList serviceIds = QStringList() << AFC_SERVICE_NAME;
            if (!CreateClient(op, serviceIds))
                return;

            lockdownd_service_descriptor_t service = GetService(op, serviceIds);
            if (!service)
                return;

            afc_error_t aerr = afc_client_new(m_clients[op]->device, service, &m_clients[op]->afc);
            if (aerr != AFC_E_SUCCESS)
                emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + serviceIds.join(", ") + " client! " + QString::number(aerr));

            //call function pass from params
            action(m_clients[op]->afc);
        }
        //clear instances
        RemoveClient(op);
    });
}
