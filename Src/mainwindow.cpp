#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <iostream>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //get devices list sample
    idevice_info_t *dev_list = NULL;
    int i;
    if (idevice_get_device_list_extended(&dev_list, &i) < 0)
    {
        std::cout << "ERROR: Unable to retrieve device list!" << std::endl;
    }
    else
    {
        for (i = 0; dev_list[i] != NULL; i++)
        {
            std::cout << dev_list[i]->udid;
            if (dev_list[i]->conn_type == CONNECTION_NETWORK)
            {
                std::cout << " (Network)" << std::endl;
            }
            else
            {
                std::cout << " (USB)" << std::endl;
            }
        }
        idevice_device_list_extended_free(dev_list);
    }

    //read from resources sample
    QFile mFile(":res/info.json");
    if(!mFile.open(QFile::ReadOnly | QFile::Text))
    {
        std::cout << "Can't access file from resource!" << std::endl;
        return;
    }
    QTextStream in(&mFile);
    QJsonDocument doc = QJsonDocument::fromJson(in.readAll().toUtf8());
    std::cout << doc["status"].toString().toStdString() << std::endl;
    mFile.close();
}

MainWindow::~MainWindow()
{
    delete ui;
}
