#include "devicebridge.h"


void DeviceBridge::GetAccessibleStorage(QString startPath, QString bundleId)
{
    afc_filemanager_action([=, this](afc_client_t& afc){
        m_accessibleStorage.clear();
        emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::FETCH, 50, bundleId);
        afc_traverse_recursive(afc, startPath.toStdString().c_str());
        emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::FETCH, 100, bundleId);
        emit AccessibleStorageReceived(m_accessibleStorage);
    }, bundleId);
}

void DeviceBridge::PushToStorage(QString localPath, QString devicePath, QString bundleId)
{
    afc_filemanager_action([=, this](afc_client_t& afc){
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

void DeviceBridge::PullFromStorage(QString devicePath, QString localPath, QString bundleId)
{
    afc_filemanager_action([=, this](afc_client_t& afc){
        int percentage = 0;
        int CHUNK_SIZE = 8192;
        uint64_t handle = 0;
        afc_error_t err = afc_file_open(afc, devicePath.toUtf8().data(), AFC_FOPEN_RDONLY, &handle);
        if (err != AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL, percentage, devicePath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not open remote file: " + devicePath + "! " + QString::number(err));
            return;
        }

        FILE *f = fopen(localPath.toUtf8().data(), "wb");
        if (!f) {
            afc_file_close(afc, handle);
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::PULL, percentage, devicePath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not open local file for writing: " + localPath + "! " + QString::number(err));
            return;
        }

        char **info = NULL;
        quint64 size_bytes = 0;
        if (afc_get_file_info(afc, devicePath.toUtf8().data(), &info) == AFC_E_SUCCESS && info)
        {
            for (int j = 0; info[j]; j += 2) {
                if (std::strcmp(info[j], "st_size") == 0) {
                    // Convert string to uint64_t
                    try {
                        size_bytes = std::stoull(info[j + 1]);
                    } catch (...) {
                        size_bytes = 0;
                    }
                }
            }
            afc_dictionary_free(info);
        }

        quint64 written_bytes = 0;
        uint32_t bytes_read = 0;
        char buffer[CHUNK_SIZE];
        while (afc_file_read(afc, handle, buffer, CHUNK_SIZE, &bytes_read) == AFC_E_SUCCESS && bytes_read > 0) {
            fwrite(buffer, 1, bytes_read, f);
            written_bytes += bytes_read;
            percentage = int((float(written_bytes) / (float(size_bytes) * 2.f)) * 100.f);
            emit FileManagerChanged(GenericStatus::IN_PROGRESS, FileOperation::PULL, percentage, devicePath);
        }

        fclose(f);
        afc_file_close(afc, handle);
        emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::PULL, percentage, devicePath);
    }, bundleId);
}

void DeviceBridge::DeleteFromStorage(QString devicePath, QString bundleId)
{
    afc_filemanager_action([=, this](afc_client_t& afc){
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
    afc_filemanager_action([=, this](afc_client_t& afc){
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
    afc_filemanager_action([=, this](afc_client_t& afc){
        afc_error_t err = afc_rename_path(afc, oldPath.toUtf8().data(), newPath.toUtf8().data());
        if (err == AFC_E_SUCCESS) {
            emit FileManagerChanged(GenericStatus::SUCCESS, FileOperation::RENAME, 100, newPath);
        } else {
            emit FileManagerChanged(GenericStatus::FAILED, FileOperation::RENAME, 100, newPath);
            emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Failed to rename: " + oldPath + " to " + newPath + "! " + QString::number(err));
        }
    }, bundleId);
}

void DeviceBridge::afc_filemanager_action(std::function<void(afc_client_t &afc)> action, const QString& bundleId)
{
    AsyncManager::Get()->StartAsyncRequest([=, this]() {
        if (!bundleId.isEmpty()) {
            QStringList serviceIds = QStringList() << HOUSE_ARREST_SERVICE_NAME;
            StartLockdown(!m_houseArrest, serviceIds, [this, bundleId](QString& service_id, lockdownd_service_descriptor_t& service){
                house_arrest_error_t err = house_arrest_client_new(m_device, service, &m_houseArrest);
                if (err != HOUSE_ARREST_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(err));

                err = house_arrest_send_command(m_houseArrest, "VendContainer", bundleId.toUtf8().data());
                if (err != HOUSE_ARREST_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Access Denied to " + bundleId + "'s VendContainer client! " + QString::number(err));

                afc_error_t aerr = afc_client_new_from_house_arrest_client(m_houseArrest, &m_fileManager);
                if (aerr != AFC_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to afc with " + service_id + " client! " + QString::number(aerr));
            });
        } else {
            QStringList serviceIds = QStringList() << AFC_SERVICE_NAME;
            StartLockdown(!m_fileManager, serviceIds, [this](QString& service_id, lockdownd_service_descriptor_t& service){
                afc_error_t aerr = afc_client_new(m_device, service, &m_fileManager);
                if (aerr != AFC_E_SUCCESS)
                    emit MessagesReceived(MessagesType::MSG_ERROR, "ERROR: Could not connect to " + service_id + " client! " + QString::number(aerr));
            });
        }

        //call function pass from params
        action(m_fileManager);

        if (m_fileManager)
        {
            afc_client_free(m_fileManager);
            m_fileManager = nullptr;
        }
        if (m_houseArrest)
        {
            house_arrest_client_free(m_houseArrest);
            m_houseArrest = nullptr;
        }
    });
}