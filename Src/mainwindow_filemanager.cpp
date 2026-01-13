#include "mainwindow.h"
#include "qmessagebox.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "userconfigs.h"
#include <QJsonObject>
#include <QFileInfo>
#include <QHeaderView>
#include <QStyle>
#include <QInputDialog>
#include <QDir>

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
        int totalWidth = ui->fileBrowserTree->width();
        if (totalWidth > 0) {
            int nameWidth = (totalWidth * 3) / 4;
            ui->fileBrowserTree->header()->resizeSection(0, nameWidth);
            ui->fileBrowserTree->header()->resizeSection(1, totalWidth - nameWidth);
        }
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

void MainWindow::OnStorageChanged(QString storage)
{
    if (storage.contains("User's Data", Qt::CaseInsensitive))
    {
        DeviceBridge::Get()->GetAccessibleStorage("/");
    }
    else
    {
        DeviceBridge::Get()->GetAccessibleStorage("/", storage);
    }
}

void MainWindow::OnAccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty> contents)
{
    m_cachedFiles = contents;// cache file property
    if (!m_fileManagerModel)
        SetupFileManagerUI();

    int cachedNameWidth = ui->fileBrowserTree->header()->sectionSize(0);
    int cachedSizeWidth = ui->fileBrowserTree->header()->sectionSize(1);
    if (cachedNameWidth > 0 && cachedSizeWidth > 0) {
        m_fileManagerNameWidth = cachedNameWidth;
        m_fileManagerSizeWidth = cachedSizeWidth;
    }

    m_fileManagerModel->clear();
    m_fileManagerModel->setHorizontalHeaderLabels(QStringList() << "Name" << "Size");

    QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
    auto formatSize = [](quint64 bytes) -> QString
    {
        return BytesToString(bytes);
    };

    QMap<QString, QStandardItem*> pathMap;
    QStandardItem *rootItem = new QStandardItem(dirIcon, "/");
    QStandardItem *rootSizeItem = new QStandardItem("");
    rootItem->setData("/", Qt::UserRole);
    m_fileManagerModel->appendRow(QList<QStandardItem*>() << rootItem << rootSizeItem);
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

    ui->fileBrowserTree->expandToDepth(0);
    ui->fileBrowserTree->header()->setSectionResizeMode(QHeaderView::Interactive);
    ui->fileBrowserTree->header()->setStretchLastSection(false);
    if (m_fileManagerNameWidth > 0 && m_fileManagerSizeWidth > 0) {
        ui->fileBrowserTree->header()->resizeSection(0, m_fileManagerNameWidth);
        ui->fileBrowserTree->header()->resizeSection(1, m_fileManagerSizeWidth);
    }
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

        if (operation != FileOperation::FETCH)
            OnRefreshFileBrowserClicked();
    }

    ui->statusbar->showMessage(message);
}

void MainWindow::OnRefreshFileBrowserClicked()
{
    OnStorageChanged(ui->storageOption->currentText());
}

void MainWindow::OnPullFileClicked()
{
}

void MainWindow::OnPushFileClicked()
{
}

void MainWindow::OnDeleteFileClicked()
{
}

void MainWindow::OnRenameFileClicked()
{
    FileManagerAction([this](QString& initialText, QString& storageAccess){
        bool ok;
        QString text = QInputDialog::getText(this, "Rename", "Current: " + initialText, QLineEdit::Normal, initialText, &ok);
        if (ok) {
            DeviceBridge::Get()->RenameToStorage(initialText, text, storageAccess);
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

}
