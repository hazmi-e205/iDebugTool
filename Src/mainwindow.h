#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QTimer>
#include "devicebridge.h"
#include "customkeyfiler.h"
#include "textviewer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class AppInfo;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void SetupDevicesTable();
    void SetupLogsTable();
    void UpdateLogsFilter();
    void AddLogToTable(LogPacket log);
    void UpdateStatusbar();
    void UpdateInfoWidget();
    Ui::MainWindow *ui;
    QStandardItemModel *m_devicesModel;
    QStandardItemModel *m_logModel;
    AppInfo *m_appInfo;
    float m_ratioTopWidth;
    float m_topWidth;
    QString m_currentUdid;
    QString m_currentFilter;
    QString m_pidFilter;
    QString m_excludeFilter;
    QString m_choosenBundleId;
    std::vector<LogPacket> m_liveLogs;
    std::map<QString, QJsonDocument> m_infoCache, m_installedApps;
    QTimer *m_scrollTimer;
    CustomKeyFilter *m_eventFilter;
    unsigned int m_maxCachedLogs, m_maxShownLogs, m_scrollInterval;
    TextViewer *m_textDialog;

private slots:
    void OnTopSplitterMoved(int pos, int index);
    void OnDevicesTableClicked(QModelIndex index);
    void OnRefreshClicked();
    void OnUpdateDevices(std::map<QString, idevice_connection_type> devices);
    void OnDeviceInfoReceived(QJsonDocument info);
    void OnSystemLogsReceived(LogPacket log);
    void OnTextFilterChanged(QString text);
    void OnPidFilterChanged(QString text);
    void OnExcludeFilterChanged(QString text);
    void OnAutoScrollChecked(int state);
    void OnClearClicked();
    void OnSaveClicked();
    void OnClickedEvent(QObject* object);
    void OnInstallClicked();
    void OnUninstallClicked();
    void OnScrollTimerTick();
    void OnConfigureClicked();
    void OnSleepClicked();
    void OnShutdownClicked();
    void OnRestartClicked();
    void OnBundleIdChanged(QString text);
    void OnSystemInfoClicked();
    void OnAppInfoClicked();
};
#endif // MAINWINDOW_H
