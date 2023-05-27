#include "selfupdater.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SelfUpdater w;
    w.show();
    return a.exec();
}
