#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <iostream>
#include <string>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    idevice_info_t *dev_list = NULL;
    int i;
    int include_usb = 1;
    int include_network = 1;

    if (idevice_get_device_list_extended(&dev_list, &i) < 0) {
        std::cout << "ERROR: Unable to retrieve device list!" << std::endl;
    }
    else {
        for (i = 0; dev_list[i] != NULL; i++) {
            if (dev_list[i]->conn_type == CONNECTION_USBMUXD && !include_usb) continue;
            if (dev_list[i]->conn_type == CONNECTION_NETWORK && !include_network) continue;
            std::cout<< std::string(APP_STATUS) << " " << dev_list[i]->udid;

            if (include_usb && include_network) {
                if (dev_list[i]->conn_type == CONNECTION_NETWORK) {
                    std::cout << " (Network)" << std::endl;
                } else {
                    std::cout << " (USB)" << std::endl;
                }
            }
        }
        idevice_device_list_extended_free(dev_list);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
