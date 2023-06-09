#include "mainwindow.h"
#include <QApplication>
#include <QBreakpadHandler.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QBreakpadInstance.setDumpPath("crashes");
    MainWindow w;
    w.show();
    return a.exec();
}
