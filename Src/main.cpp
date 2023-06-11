#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#if defined(WIN32)
#include "exchndl.h"
#else
#include "asmCrashReport.h"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if defined(WIN32)
    QString filename = QCoreApplication::applicationName() + "_CrashReports.txt";
    ExcHndlSetLogFileNameA(filename.toUtf8().data());
    ExcHndlInit();
#else
    asmCrashReport::setSignalHandler("crashes", [] (const QString &inFileName, bool inSuccess) {
        QString  message;
        if (inSuccess)
            message = QString("Sorry, %1 has crashed. A log file was written to:\n\n%2\n\n").arg(QCoreApplication::applicationName(), inFileName);
        else
            message = QString("Sorry, %1 has crashed and we could not write a log file to:\n\n%2\n\n").arg(QCoreApplication::applicationName(), inFileName);

        message += "Please post your issue to https://github.com/hazmi-e205/iDebugTool/issues";
        QMessageBox::critical(nullptr, QString("%1 Crashed").arg(QCoreApplication::applicationName()), message);
    });
#endif
    MainWindow w;
    w.show();
    return a.exec();
}
