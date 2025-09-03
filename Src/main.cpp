#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#if defined(MINGW_REPORTER)
#include "exchndl.h"
#include "utils.h"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if defined(MINGW_REPORTER)
    QString filename = QCoreApplication::applicationName() + "_CrashReports.txt";
    CheckDrMinGWReports(filename, [](QString filepath, int count){
        QString  message = QString("Sorry, %1 has crashed in %2 time(s). A log file was written to:\n\n%3\n\n").arg(QCoreApplication::applicationName()).arg(count).arg(filepath);
        message += "Please post your issue to https://github.com/hazmi-e205/iDebugTool/issues";
        QMessageBox::critical(nullptr, QString("%1 Crashed").arg(QCoreApplication::applicationName()), message);
    });
    ExcHndlSetLogFileNameA(filename.toUtf8().data());
    ExcHndlInit();
#endif
    MainWindow w;
    w.show();
    return a.exec();
}
