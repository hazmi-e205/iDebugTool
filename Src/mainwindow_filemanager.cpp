#include "mainwindow.h"
#include "qmessagebox.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include <QJsonObject>
#include <QFileInfo>
#include <QStyle>
#include <QInputDialog>
#include <QDir>
#include <QRegularExpression>

void MainWindow::CacheHeaderSizes(QHeaderView *header, int &nameWidth, int &sizeWidth)
{
    if (!header)
        return;

    int cachedNameWidth = header->sectionSize(0);
    int cachedSizeWidth = header->sectionSize(1);
    if (cachedNameWidth > 0 && cachedSizeWidth > 0) {
        nameWidth = cachedNameWidth;
        sizeWidth = cachedSizeWidth;
    }
}

void MainWindow::RestoreHeaderSizes(QHeaderView *header, int nameWidth, int sizeWidth)
{
    if (!header)
        return;

    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    if (nameWidth > 0 && sizeWidth > 0) {
        header->resizeSection(0, nameWidth);
        header->resizeSection(1, sizeWidth);
    }
}

void MainWindow::SetupFileManagerUI()
{
    if (!m_fileManagerModel) {
        m_fileManagerModel = new QStandardItemModel();
        m_fileManagerModel->setHorizontalHeaderLabels(QStringList() << "Name" << "Size");
        ui->fileBrowserTree->setModel(m_fileManagerModel);
        ui->fileBrowserTree->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->fileBrowserTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->fileBrowserTree->setRootIsDecorated(true);
        ui->fileBrowserTree->setItemsExpandable(true);
        ui->fileBrowserTree->setExpandsOnDoubleClick(true);
        ui->fileBrowserTree->header()->setSectionResizeMode(QHeaderView::Interactive);
        ui->fileBrowserTree->header()->setStretchLastSection(false);
        int totalWidth = 700; // hardcoded since getting total width not accurate
        if (totalWidth > 0) {
            int nameWidth = (totalWidth * 3) / 4;
            ui->fileBrowserTree->header()->resizeSection(0, nameWidth);
            ui->fileBrowserTree->header()->resizeSection(1, totalWidth - nameWidth);
            m_fileManagerNameWidth = nameWidth;
            m_fileManagerSizeWidth = totalWidth - nameWidth;
        }
        ResetFileBrowser();
        m_loadingFileOperation->close();
    }
    connect(ui->storageOption, SIGNAL(textActivated(QString)), this, SLOT(OnStorageChanged(QString)));
    connect(ui->refreshFileBtn, SIGNAL(pressed()), this, SLOT(OnRefreshFileBrowserClicked()));
    connect(ui->makeFolderBtn, SIGNAL(pressed()), this, SLOT(OnMakeFolderClicked()));
    connect(ui->pullFileBtn, SIGNAL(pressed()), this, SLOT(OnPullFileClicked()));
    connect(ui->pushFileBtn, SIGNAL(pressed()), this, SLOT(OnPushFileClicked()));
    connect(ui->deleteFileBtn, SIGNAL(pressed()), this, SLOT(OnDeleteFileClicked()));
    connect(ui->renameFileBtn, SIGNAL(pressed()), this, SLOT(OnRenameFileClicked()));
    connect(ui->searchFileEdit, SIGNAL(textChanged(QString)), this, SLOT(OnFileFilterChanged(QString)));
    connect(DeviceBridge::Get(), SIGNAL(AccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty>)), this, SLOT(OnAccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty>)));
    connect(DeviceBridge::Get(), SIGNAL(FileManagerChanged(GenericStatus, FileOperation, int, QString)), this, SLOT(OnFileManagerChanged(GenericStatus, FileOperation, int, QString)));
}

void MainWindow::FileManagerAction(std::function<void(QString&,QString&)> action, bool asFolder)
{
    // storage name
    QString storage = ui->storageOption->currentText();
    storage = storage.contains("User's Data", Qt::CaseInsensitive) ? "" : storage;

    // initial path
    QModelIndex selectedPath = ui->fileBrowserTree->currentIndex();
    if (!selectedPath.isValid()) {
        QMessageBox::critical(this, "Error", "Please choose the destination on File Manager's Browser", QMessageBox::Ok);
        return;
    }
    QString initialText = selectedPath.data(Qt::UserRole).toString();
    if (!m_cachedFiles[initialText].isDirectory && asFolder)
    {
        QFileInfo fileInfo(initialText);
        initialText = fileInfo.dir().absolutePath();
    }
    if (asFolder)
        initialText += "/";

    // custom call
    action(initialText, storage);
}

void MainWindow::ResetFileBrowser()
{
    if (!m_fileManagerModel)
    {
        SetupFileManagerUI();
        return;
    }

    CacheHeaderSizes(ui->fileBrowserTree->header(), m_fileManagerNameWidth, m_fileManagerSizeWidth);

    m_fileManagerModel->clear();
    m_fileManagerModel->setHorizontalHeaderLabels(QStringList() << "Name" << "Size");

    QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QStandardItem *rootItem = new QStandardItem(dirIcon, "/");
    QStandardItem *rootSizeItem = new QStandardItem("");
    rootItem->setData("/", Qt::UserRole);
    m_fileManagerModel->appendRow(QList<QStandardItem*>() << rootItem << rootSizeItem);

    RestoreHeaderSizes(ui->fileBrowserTree->header(), m_fileManagerNameWidth, m_fileManagerSizeWidth);
}

void MainWindow::SaveExpandedPaths(const QString& bundleId)
{
    if (!m_fileManagerModel)
        return;

    QSet<QString> expanded;
    std::function<void(QStandardItem*)> collect;
    collect = [&](QStandardItem* item) {
        if (!item)
            return;
        if (ui->fileBrowserTree->isExpanded(m_fileManagerModel->indexFromItem(item))) {
            QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                expanded.insert(path);
        }
        for (int row = 0; row < item->rowCount(); ++row)
            collect(item->child(row, 0));
    };

    for (int row = 0; row < m_fileManagerModel->rowCount(); ++row)
        collect(m_fileManagerModel->item(row, 0));

    m_expandedPathsByBundle[bundleId] = expanded;
}

void MainWindow::RestoreExpandedPaths(const QString& bundleId)
{
    if (!m_fileManagerModel || !m_expandedPathsByBundle.contains(bundleId))
        return;

    const QSet<QString>& expanded = m_expandedPathsByBundle[bundleId];
    std::function<void(QStandardItem*)> applyExpand;
    applyExpand = [&](QStandardItem* item) {
        if (!item)
            return;
        QString path = item->data(Qt::UserRole).toString();
        if (expanded.contains(path))
            ui->fileBrowserTree->expand(m_fileManagerModel->indexFromItem(item));
        for (int row = 0; row < item->rowCount(); ++row)
            applyExpand(item->child(row, 0));
    };

    for (int row = 0; row < m_fileManagerModel->rowCount(); ++row)
        applyExpand(m_fileManagerModel->item(row, 0));
}

void MainWindow::OnStorageChanged(QString storage)
{
    if (storage.contains("User's Data", Qt::CaseInsensitive))
    {
        m_currentFileManagerBundleId = "";
        DeviceBridge::Get()->GetAccessibleStorage("/");
    }
    else
    {
        m_currentFileManagerBundleId = storage;
        DeviceBridge::Get()->GetAccessibleStorage("/", storage);
    }
}

void MainWindow::OnAccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty> contents)
{
    m_cachedFiles = contents;// cache file property
    if (!m_fileManagerModel)
        SetupFileManagerUI();
    else {
        SaveExpandedPaths(m_currentFileManagerBundleId);
        ResetFileBrowser();
    }

    QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
    auto formatSize = [](quint64 bytes) -> QString
    {
        return BytesToString(bytes);
    };

    QMap<QString, QStandardItem*> pathMap;
    QStandardItem *rootItem = m_fileManagerModel->item(0, 0);
    if (!rootItem) {
        QStandardItem *rootSizeItem = new QStandardItem("");
        rootItem = new QStandardItem(dirIcon, "/");
        rootItem->setData("/", Qt::UserRole);
        m_fileManagerModel->appendRow(QList<QStandardItem*>() << rootItem << rootSizeItem);
    }
    pathMap["/"] = rootItem;

    auto ensureDirPath = [&](const QString &path)
    {
        QStringList parts = path.split("/", Qt::SkipEmptyParts);
        QString currentPath = "/";
        QStandardItem *parentItem = pathMap["/"];
        for (int i = 0; i < parts.count(); i++)
        {
            currentPath = (currentPath == "/") ? "/" + parts[i] : currentPath + "/" + parts[i];
            if (!pathMap.contains(currentPath)) {
                QStandardItem *dirItem = new QStandardItem(dirIcon, parts[i]);
                QStandardItem *dirSizeItem = new QStandardItem("");
                dirItem->setData(currentPath, Qt::UserRole);
                parentItem->appendRow(QList<QStandardItem*>() << dirItem << dirSizeItem);
                pathMap[currentPath] = dirItem;
            }
            parentItem = pathMap[currentPath];
        }
    };

    QStringList keys = contents.keys();
    keys.sort();
    foreach (const QString &path, keys)
    {
        auto prop = contents[path];
        if (prop.isDirectory) {
            ensureDirPath(path);
        }
    }

    foreach (const QString &path, keys)
    {
        auto prop = contents[path];
        if (prop.isDirectory)
            continue;

        QString parentPath = QFileInfo(path).path();
        if (parentPath.isEmpty())
            parentPath = "/";
        ensureDirPath(parentPath);

        if (!pathMap.contains(path)) {
            QStandardItem *parentItem = pathMap.value(parentPath, pathMap["/"]);
            QStandardItem *fileItem = new QStandardItem(fileIcon, QFileInfo(path).fileName());
            QStandardItem *fileSizeItem = new QStandardItem(formatSize(prop.sizeInBytes));
            fileItem->setData(path, Qt::UserRole);
            parentItem->appendRow(QList<QStandardItem*>() << fileItem << fileSizeItem);
            pathMap[path] = fileItem;
        }
    }

    if (m_expandedPathsByBundle.contains(m_currentFileManagerBundleId))
        RestoreExpandedPaths(m_currentFileManagerBundleId);
    else
        ui->fileBrowserTree->expandToDepth(0);
    // Filter the contents
    OnFileFilterChanged(ui->searchFileEdit->text());
}

void MainWindow::OnFileManagerChanged(GenericStatus status, FileOperation operation, int percentage, QString message)
{
    if (status == GenericStatus::IN_PROGRESS) {
        if (!m_loadingFileOperation->isVisible())
            m_loadingFileOperation->ShowProgress("File Operation");

        switch (operation) {
        case FileOperation::PUSH:
            message = "Push a file to " + message + " in device";
            break;
        case FileOperation::PULL:
            message = "Pull " + message + " to local directory";
            break;
        case FileOperation::RENAME:
            message = "Rename a file to " + message + " in device";
            break;
        case FileOperation::DELETE_OP:
            message = "Delete " + message + " in device";
            break;
        case FileOperation::MAKE_FOLDER:
            message = "Make folder " + message + " in device";
            break;
        case FileOperation::FETCH:
            message = "Fetch directories of " + (message.isEmpty() ? "User's Data" : message) + " in device";
            break;
        }

        m_loadingFileOperation->SetProgress(percentage, message);
        ui->statusbar->showMessage(message);
        ui->statusbar->showMessage(message + " (" + QString::number(percentage) + "%)");
    }

    if (status == GenericStatus::SUCCESS) {
        if (m_loadingFileOperation->isVisible())
            m_loadingFileOperation->close();

        if (operation != FileOperation::FETCH && operation != FileOperation::PULL)
        {
            // storage name
            QString storage = ui->storageOption->currentText();
            storage = storage.contains("User's Data", Qt::CaseInsensitive) ? "" : storage;

            // partial update
            QFileInfo fileInfo(message);
            QString dirPath = fileInfo.dir().absolutePath();
            DeviceBridge::Get()->GetAccessibleStorage(dirPath, storage, true);
        }
    }

    ui->statusbar->showMessage(message);
}

void MainWindow::OnRefreshFileBrowserClicked()
{
    OnStorageChanged(ui->storageOption->currentText());
}

void MainWindow::OnPullFileClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        if (m_cachedFiles[initialText].isDirectory || initialText.isEmpty()) {
            QMessageBox::critical(this, "Error", "Please choose a file in File Manager's Browser!", QMessageBox::Ok);
            return;
        }
        QFileInfo fileInfo(initialText);
        QString file = fileInfo.fileName();
        QString filepath = ShowBrowseDialog(BROWSE_TYPE::SAVE_FILE, "Pull a file to device", this, "", file);
        if (!filepath.isEmpty())
            DeviceBridge::Get()->PullFromStorage(initialText, filepath, storageAccess);
    }, false);
}

void MainWindow::OnPushFileClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        QString filepath = ShowBrowseDialog(BROWSE_TYPE::OPEN_FILE, "Push a file to device", this);
        if (!filepath.isEmpty()) {
            QFileInfo fileInfo(filepath);
            QString file = fileInfo.fileName();
            DeviceBridge::Get()->PushToStorage(filepath, initialText + file, storageAccess);
        }
    }, true);
}

void MainWindow::OnDeleteFileClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Delete");
        msgBox.setText("Are you sure to delete " + initialText + " from device?");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);

        int ret = msgBox.exec();
        if (ret == QMessageBox::Ok) {
            DeviceBridge::Get()->DeleteFromStorage(initialText, storageAccess);
        }
        else {
            m_loadingFileOperation->close();
        }
    }, false);
}

void MainWindow::OnRenameFileClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        bool ok;
        QFileInfo fileInfo(initialText);
        QString folder = fileInfo.dir().absolutePath();
        QString file = fileInfo.fileName();
        QString text = QInputDialog::getText(this, "Rename", "Current: " + initialText, QLineEdit::Normal, file, &ok);
        if (ok) {
            DeviceBridge::Get()->RenameToStorage(initialText, folder + "/" + text, storageAccess);
        }
        else {
            m_loadingFileOperation->close();
        }
    }, false);
}

void MainWindow::OnMakeFolderClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        bool ok;
        QString text = QInputDialog::getText(this, "Make Folder", "Path: " + initialText, QLineEdit::Normal, "", &ok);
        if (ok) {
            DeviceBridge::Get()->MakeDirectoryToStorage(initialText + text, storageAccess);
        }
        else {
            m_loadingFileOperation->close();
        }
    }, true);
}

void MainWindow::OnFileFilterChanged(QString filter)
{
    if (!m_fileManagerModel)
        return;

    QString trimmed = filter.trimmed();
    QString regex;
    if (!trimmed.isEmpty())
        regex = "(?i)" + QRegularExpression::escape(trimmed);

    auto matchesItem = [&](QStandardItem *item) -> bool
    {
        if (!item)
            return false;
        if (trimmed.isEmpty())
            return true;
        return !FindRegex(item->text(), regex).isEmpty();
    };

    auto isDirectoryItem = [&](QStandardItem *item) -> bool
    {
        if (!item)
            return false;
        QString path = item->data(Qt::UserRole).toString();
        return m_cachedFiles.contains(path) && m_cachedFiles[path].isDirectory;
    };

    std::function<void(QStandardItem*)> showAllChildren;
    showAllChildren = [&](QStandardItem *item)
    {
        if (!item)
            return;
        for (int row = 0; row < item->rowCount(); row++)
        {
            QStandardItem *childItem = item->child(row, 0);
            ui->fileBrowserTree->setRowHidden(row, item->index(), false);
            showAllChildren(childItem);
        }
    };

    std::function<bool(QStandardItem*)> applyFilter;
    applyFilter = [&](QStandardItem *item) -> bool
    {
        if (!item)
            return false;

        bool itemMatch = matchesItem(item);
        if (itemMatch && isDirectoryItem(item)) {
            showAllChildren(item);
            return true;
        }

        bool anyChildMatch = false;
        for (int row = 0; row < item->rowCount(); row++)
        {
            QStandardItem *childItem = item->child(row, 0);
            bool childMatch = applyFilter(childItem);
            ui->fileBrowserTree->setRowHidden(row, item->index(), !childMatch);
            anyChildMatch = anyChildMatch || childMatch;
        }

        bool visible = itemMatch || anyChildMatch;
        if (!trimmed.isEmpty() && anyChildMatch && !itemMatch)
            ui->fileBrowserTree->expand(item->index());
        return visible;
    };

    for (int row = 0; row < m_fileManagerModel->rowCount(); row++)
    {
        QStandardItem *item = m_fileManagerModel->item(row, 0);
        bool match = applyFilter(item);
        ui->fileBrowserTree->setRowHidden(row, QModelIndex(), !match);
    }
}
