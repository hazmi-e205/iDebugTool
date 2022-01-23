#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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

private:
    Ui::MainWindow *ui;
    AppInfo *m_appInfo;

private slots:
    void OnUpdateDevices(std::vector<Device> devices);
    void OnDeviceInfo(QJsonDocument info);
};
#endif // MAINWINDOW_H
