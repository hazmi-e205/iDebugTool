#include "mainwindow.h"
#include "asmCrashReport.h"
#include <QApplication>
#include <QMessageBox>
#include <QBreakpadHandler.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QBreakpadInstance.setDumpPath("breakpad");
//    QBreakpadInstance.setUploadUrl(QUrl("https://submit.backtrace.io/janur/dc724c315fea792a08b67023b0146679560b87e352f7a84e383e52c7143ef160/minidump"));
//    asmCrashReport::setSignalHandler("crashes", [] (const QString &inFileName, bool inSuccess) {
//        QString  message;
//        if (inSuccess)
//            message = QString("Sorry, %1 has crashed. A log file was written to:\n\n%2\n\n").arg(QCoreApplication::applicationName(), inFileName);
//        else
//            message = QString("Sorry, %1 has crashed and we could not write a log file to:\n\n%2\n\n").arg(QCoreApplication::applicationName(), inFileName);

//        message += "Please post your issue to https://github.com/hazmi-e205/iDebugTool/issues";
//        QMessageBox::critical(nullptr, QString("%1 Crashed").arg(QCoreApplication::applicationName()), message);
//    });
    MainWindow w;
    w.show();
    return a.exec();
}
