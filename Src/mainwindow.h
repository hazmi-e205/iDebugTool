#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QResizeEvent>
#include <QStandardItemModel>
#include "devicebridge.h"

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

private:
    void SetupDevicesTable();
    void SetupLogsTable();
    void UpdateLogsFilter();
    void AddLogToTable(LogPacket log);
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
    std::vector<LogPacket> m_liveLogs;
    std::map<QString, QJsonDocument> m_infoCache;
    bool m_autoScroll;

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
};
#endif // MAINWINDOW_H
