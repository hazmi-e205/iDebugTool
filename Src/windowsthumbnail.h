#ifndef WINDOWSTHUMBNAIL_H
#define WINDOWSTHUMBNAIL_H

#include <QPixmap>
#include <QSize>
#include <QString>

QPixmap LoadWindowsShellThumbnail(const QString& localPath, const QSize& size);
QPixmap LoadFfmpegVideoThumbnail(const QString& localPath, const QSize& size);

#endif // WINDOWSTHUMBNAIL_H
