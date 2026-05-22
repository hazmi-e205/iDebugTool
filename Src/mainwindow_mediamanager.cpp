#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils.h"

#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QStyledItemDelegate>
#include <QUrl>

namespace {
constexpr int kMediaRoleDevicePath = Qt::UserRole + 1;
constexpr int kMediaRoleIsVideo    = Qt::UserRole + 2;
constexpr int kMediaRoleSizeBytes  = Qt::UserRole + 3;
constexpr int kMediaRoleThumbLoaded = Qt::UserRole + 5;
constexpr int kMediaRoleModTimeNs  = Qt::UserRole + 6;

const QSize kMediaThumbSize(192, 144);

class MediaItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        bool isVideo = index.data(kMediaRoleIsVideo).toBool();
        if (!isVideo) return;

        painter->save();
        QFont f = painter->font();
        f.setPointSize(qMax(7, f.pointSize() - 1));
        f.setBold(true);
        painter->setFont(f);

        const QString label = QStringLiteral("VIDEO");
        QFontMetrics fm(f);
        const int padX = 4, padY = 2;
        int textW = fm.horizontalAdvance(label);
        int textH = fm.height();

        QRect r = option.rect;
        QRect badge(r.right() - textW - padX * 2 - 4,
                    r.bottom() - textH - padY * 2 - 24,
                    textW + padX * 2,
                    textH + padY * 2);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(QColor(0, 0, 0, 170));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(badge, 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, label);
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option); Q_UNUSED(index);
        return QSize(kMediaThumbSize.width() + 16, kMediaThumbSize.height() + 44);
    }
};
} // namespace

// --------------------------------------------------------------------------
// MainWindow methods
// --------------------------------------------------------------------------

void MainWindow::SetupMediaManagerUI()
{
    if (m_mediaModel)
        return;

    qRegisterMetaType<DeviceBridge::MediaEntry>("DeviceBridge::MediaEntry");
    qRegisterMetaType<QList<DeviceBridge::MediaEntry>>("QList<DeviceBridge::MediaEntry>");

    m_mediaModel = new QStandardItemModel(this);
    ui->mediaListView->setModel(m_mediaModel);
    ui->mediaListView->setViewMode(QListView::IconMode);
    ui->mediaListView->setResizeMode(QListView::Adjust);
    ui->mediaListView->setMovement(QListView::Static);
    ui->mediaListView->setUniformItemSizes(true);
    ui->mediaListView->setIconSize(kMediaThumbSize);
    ui->mediaListView->setGridSize(QSize(kMediaThumbSize.width() + 16, kMediaThumbSize.height() + 44));
    ui->mediaListView->setSpacing(4);
    ui->mediaListView->setItemDelegate(new MediaItemDelegate(this));
    ui->mediaListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->mediaListView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->mediaRefreshBtn,  &QPushButton::clicked, this, &MainWindow::OnMediaRefreshClicked);
    connect(ui->mediaOpenBtn,     &QPushButton::clicked, this, &MainWindow::OnMediaOpenClicked);
    connect(ui->mediaDownloadBtn, &QPushButton::clicked, this, &MainWindow::OnMediaDownloadClicked);
    connect(ui->mediaDeleteBtn,   &QPushButton::clicked, this, &MainWindow::OnMediaDeleteClicked);
    connect(ui->mediaSortCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::OnMediaSortChanged);
    connect(ui->mediaListView,    &QListView::doubleClicked, this, &MainWindow::OnMediaItemDoubleClicked);

    connect(DeviceBridge::Get(), &DeviceBridge::MediaFilesEnumerated, this, &MainWindow::OnMediaFilesEnumerated);
    connect(DeviceBridge::Get(), &DeviceBridge::MediaFileFetched, this, &MainWindow::OnMediaFileFetched);
}

QString MainWindow::MediaCacheKey(const QString& devicePath) const
{
    QString udid = m_mediaCurrentDeviceUdid.isEmpty()
        ? DeviceBridge::Get()->GetCurrentUdid()
        : m_mediaCurrentDeviceUdid;
    QByteArray hash = QCryptographicHash::hash((udid + "|" + devicePath).toUtf8(), QCryptographicHash::Sha1);
    return hash.toHex();
}

QString MainWindow::MediaCacheThumbPath(const QString& devicePath) const
{
    return GetDirectory(DIRECTORY_TYPE::THUMBNAIL) + MediaCacheKey(devicePath) + ".jpg";
}

QString MainWindow::MediaCacheFullPath(const QString& devicePath) const
{
    const QString suffix = QFileInfo(devicePath).suffix();
    QString p = GetDirectory(DIRECTORY_TYPE::TEMP) + "media_" + MediaCacheKey(devicePath);
    if (!suffix.isEmpty()) p += "." + suffix;
    return p;
}

void MainWindow::RequestMediaEnumeration()
{
    if (m_mediaEnumerating)
        return;
    m_mediaCurrentDeviceUdid = DeviceBridge::Get()->GetCurrentUdid();
    if (m_mediaCurrentDeviceUdid.isEmpty()) {
        ui->statusbar->showMessage("No device connected");
        return;
    }
    m_mediaThumbToOriginal.clear();
    if (m_mediaModel)
        m_mediaModel->clear();
    m_mediaEnumerating = true;
    ui->statusbar->showMessage("Scanning DCIM...");
    DeviceBridge::Get()->EnumerateMediaFiles("/DCIM/");
}

void MainWindow::OnMediaRefreshClicked()
{
    RequestMediaEnumeration();
}

QStringList MainWindow::SelectedMediaDevicePaths() const
{
    QStringList paths;
    if (!m_mediaModel || !ui->mediaListView->selectionModel())
        return paths;
    const QModelIndexList sel = ui->mediaListView->selectionModel()->selectedIndexes();
    for (const QModelIndex& idx : sel) {
        QStandardItem* it = m_mediaModel->itemFromIndex(idx);
        if (!it) continue;
        QString p = it->data(kMediaRoleDevicePath).toString();
        if (!p.isEmpty() && !paths.contains(p))
            paths.append(p);
    }
    return paths;
}

static void ApplyThumbnailToItem(QStandardItem* it, const QString& localJpgPath)
{
    QPixmap pm(localJpgPath);
    if (pm.isNull()) return;
    it->setIcon(QIcon(pm));
    it->setData(true, kMediaRoleThumbLoaded);
}

void MainWindow::OnMediaFilesEnumerated(QList<DeviceBridge::MediaEntry> entries)
{
    m_mediaEnumerating = false;
    if (!m_mediaModel)
        return;
    m_mediaModel->clear();
    m_mediaThumbToOriginal.clear();

    QIcon imageIcon = style()->standardIcon(QStyle::SP_FileIcon);
    QIcon videoIcon = style()->standardIcon(QStyle::SP_MediaPlay);

    QList<QPair<QString, QString>> batchRequests;
    batchRequests.reserve(entries.size());

    QDir().mkpath(GetDirectory(DIRECTORY_TYPE::THUMBNAIL));

    for (const auto& e : entries) {
        QStandardItem* it = new QStandardItem(QFileInfo(e.devicePath).fileName());
        it->setData(e.devicePath, kMediaRoleDevicePath);
        it->setData(e.isVideo, kMediaRoleIsVideo);
        it->setData(static_cast<qulonglong>(e.sizeInBytes), kMediaRoleSizeBytes);
        it->setData(false, kMediaRoleThumbLoaded);
        it->setData(QVariant::fromValue<qint64>(e.modTimeNs), kMediaRoleModTimeNs);
        it->setIcon(e.isVideo ? videoIcon : imageIcon);
        it->setToolTip(e.devicePath);
        m_mediaModel->appendRow(it);

        if (e.thumbDevicePath.isEmpty())
            continue;

        const QString thumbCachePath = MediaCacheThumbPath(e.devicePath);
        if (QFileInfo::exists(thumbCachePath)) {
            ApplyThumbnailToItem(it, thumbCachePath);
            continue;
        }

        m_mediaThumbToOriginal.insert(e.thumbDevicePath, e.devicePath);
        batchRequests.append({e.thumbDevicePath, thumbCachePath});
    }

    ApplyMediaSort();

    ui->statusbar->showMessage(QString("%1 items").arg(m_mediaModel->rowCount()));

    if (!batchRequests.isEmpty())
        DeviceBridge::Get()->FetchMediaFiles(batchRequests);
}

void MainWindow::ApplyMediaSort()
{
    if (!m_mediaModel)
        return;
    m_mediaModel->setSortRole(kMediaRoleModTimeNs);
    const Qt::SortOrder order = (m_mediaSortOrder == MediaSortOrder::NewestFirst)
        ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_mediaModel->sort(0, order);
}

void MainWindow::OnMediaSortChanged(int index)
{
    m_mediaSortOrder = (index == 0) ? MediaSortOrder::NewestFirst : MediaSortOrder::OldestFirst;
    ApplyMediaSort();
}

void MainWindow::OnMediaFileFetched(QString devicePath, QString localPath, bool success)
{
    // devicePath here is the PhotoData thumb path. Look up the original media
    // path to find the tile it belongs to.
    auto it = m_mediaThumbToOriginal.constFind(devicePath);
    if (it == m_mediaThumbToOriginal.constEnd()) {
        // Either it was a double-click full-file fetch (handled elsewhere via
        // one-shot connection), or it's stale from a previous refresh.
        return;
    }
    const QString originalPath = it.value();

    if (success && !localPath.isEmpty() && m_mediaModel) {
        for (int i = 0; i < m_mediaModel->rowCount(); ++i) {
            QStandardItem* item = m_mediaModel->item(i);
            if (!item || item->data(kMediaRoleDevicePath).toString() != originalPath)
                continue;
            ApplyThumbnailToItem(item, localPath);
            break;
        }
    }
}

void MainWindow::OnMediaItemDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid() || !m_mediaModel) return;
    QStandardItem* it = m_mediaModel->itemFromIndex(index);
    if (!it) return;

    const QString devicePath = it->data(kMediaRoleDevicePath).toString();
    const QString fullCache = MediaCacheFullPath(devicePath);

    auto openIt = [fullCache, this]() {
        if (QFileInfo::exists(fullCache))
            QDesktopServices::openUrl(QUrl::fromLocalFile(fullCache));
        else
            ui->statusbar->showMessage("Failed to fetch media file");
    };

    if (QFileInfo::exists(fullCache)) {
        openIt();
        return;
    }

    ui->statusbar->showMessage("Fetching " + QFileInfo(devicePath).fileName() + "...");

    // One-shot connection that opens the file when fetch completes.
    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(DeviceBridge::Get(), &DeviceBridge::MediaFileFetched, this,
        [this, devicePath, openIt, conn](const QString& dp, const QString& /*lp*/, bool success) {
            if (dp != devicePath) return;
            QObject::disconnect(*conn);
            delete conn;
            if (success) openIt();
            else ui->statusbar->showMessage("Failed to fetch media file");
        });

    DeviceBridge::Get()->FetchMediaFiles({{devicePath, fullCache}});
}

void MainWindow::OnMediaOpenClicked()
{
    const QStringList paths = SelectedMediaDevicePaths();
    if (paths.isEmpty()) {
        ui->statusbar->showMessage("Select one or more items first");
        return;
    }

    QList<QPair<QString, QString>> toFetch;
    QStringList toOpenNow;
    for (const QString& devicePath : paths) {
        const QString fullCache = MediaCacheFullPath(devicePath);
        if (QFileInfo::exists(fullCache))
            toOpenNow.append(fullCache);
        else
            toFetch.append({devicePath, fullCache});
    }

    for (const QString& local : toOpenNow)
        QDesktopServices::openUrl(QUrl::fromLocalFile(local));

    if (toFetch.isEmpty())
        return;

    ui->statusbar->showMessage(QString("Fetching %1 file(s)...").arg(toFetch.size()));

    // Track which device paths still need to open after their fetch completes.
    auto pending = std::make_shared<QSet<QString>>();
    for (const auto& p : toFetch)
        pending->insert(p.first);

    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(DeviceBridge::Get(), &DeviceBridge::MediaFileFetched, this,
        [this, pending, conn](const QString& dp, const QString& lp, bool success) {
            if (!pending->contains(dp))
                return;
            pending->remove(dp);
            if (success && !lp.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(lp));
            if (pending->isEmpty()) {
                QObject::disconnect(*conn);
                delete conn;
            }
        });

    DeviceBridge::Get()->FetchMediaFiles(toFetch);
}

void MainWindow::OnMediaDownloadClicked()
{
    const QStringList paths = SelectedMediaDevicePaths();
    if (paths.isEmpty()) {
        ui->statusbar->showMessage("Select one or more items first");
        return;
    }

    const QString destDir = QFileDialog::getExistingDirectory(this, "Download media to...",
        GetDirectory(DIRECTORY_TYPE::LOCALDATA));
    if (destDir.isEmpty())
        return;

    QList<QPair<QString, QString>> pairs;
    pairs.reserve(paths.size());
    for (const QString& devicePath : paths) {
        const QString fileName = QFileInfo(devicePath).fileName();
        pairs.append({devicePath, QDir(destDir).filePath(fileName)});
    }

    DeviceBridge::Get()->PullMultipleFromStorage(pairs, "");
}

void MainWindow::OnMediaDeleteClicked()
{
    const QStringList paths = SelectedMediaDevicePaths();
    if (paths.isEmpty()) {
        ui->statusbar->showMessage("Select one or more items first");
        return;
    }

    QString message = paths.size() == 1
        ? QString("Delete '%1' from device?").arg(QFileInfo(paths.first()).fileName())
        : QString("Delete %1 items from device?").arg(paths.size());

    if (QMessageBox::question(this, "Delete media", message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    DeviceBridge::Get()->DeleteMultipleFromStorage(paths, "");

    // Optimistically remove from the model; a full refresh would also work but
    // this is snappier feedback.
    if (m_mediaModel) {
        QSet<QString> set(paths.begin(), paths.end());
        for (int i = m_mediaModel->rowCount() - 1; i >= 0; --i) {
            QStandardItem* it = m_mediaModel->item(i);
            if (it && set.contains(it->data(kMediaRoleDevicePath).toString()))
                m_mediaModel->removeRow(i);
        }
        ui->statusbar->showMessage(QString("%1 items").arg(m_mediaModel->rowCount()));
    }
}
