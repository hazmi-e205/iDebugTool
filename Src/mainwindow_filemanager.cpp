#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"
#include "userconfigs.h"
#include <QJsonObject>
#include <QFileInfo>
#include <QStyle>

void MainWindow::SetupFileManagerUI()
{
    if (!m_fileManagerModel) {
        m_fileManagerModel = new QStandardItemModel();
        m_fileManagerModel->setHorizontalHeaderLabels(QStringList() << "Name");
        ui->fileBrowserTree->setModel(m_fileManagerModel);
        ui->fileBrowserTree->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->fileBrowserTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->fileBrowserTree->setRootIsDecorated(true);
        ui->fileBrowserTree->setItemsExpandable(true);
        ui->fileBrowserTree->setExpandsOnDoubleClick(true);
    }
    connect(ui->storageOption, SIGNAL(textActivated(QString)), this, SLOT(OnStorageChanged(QString)));
    connect(DeviceBridge::Get(), SIGNAL(AccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty>)), this, SLOT(OnAccessibleStorageReceived(QMap<QString, DeviceBridge::FileProperty>)));
    connect(DeviceBridge::Get(), SIGNAL(FileManagerChanged(GenericStatus, FileOperation, int, QString)), this, SLOT(OnFileManagerChanged(GenericStatus, FileOperation, int, QString)));
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
    if (!m_fileManagerModel)
        SetupFileManagerUI();

    m_fileManagerModel->clear();
    m_fileManagerModel->setHorizontalHeaderLabels(QStringList() << "Name");

    QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);

    QMap<QString, QStandardItem*> pathMap;
    QStandardItem *rootItem = new QStandardItem(dirIcon, "/");
    rootItem->setData("/", Qt::UserRole);
    m_fileManagerModel->appendRow(rootItem);
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
                dirItem->setData(currentPath, Qt::UserRole);
                parentItem->appendRow(dirItem);
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
            fileItem->setData(path, Qt::UserRole);
            parentItem->appendRow(fileItem);
            pathMap[path] = fileItem;
        }
    }

    ui->fileBrowserTree->expandToDepth(0);
}

void MainWindow::OnFileManagerChanged(GenericStatus status, FileOperation operation, int percentage, QString message)
{
    if (status == GenericStatus::IN_PROGRESS) {
        if (!m_loadingDevice->isVisible())
            m_loadingDevice->ShowProgress("File Operation");

        m_loadingDevice->SetProgress(percentage, message);
        ui->statusbar->showMessage(message);
        ui->statusbar->showMessage(message + " (" + QString::number(percentage) + "%)");
    }

    if (status == GenericStatus::SUCCESS) {
        if (m_loadingDevice->isVisible())
            m_loadingDevice->close();
        OnStorageChanged(ui->storageOption->currentText());
    }

    ui->statusbar->showMessage(message);
}
