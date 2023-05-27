#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class SelfUpdater; }
QT_END_NAMESPACE

class SelfUpdater : public QMainWindow
{
    Q_OBJECT

public:
    SelfUpdater(QWidget *parent = nullptr);
    ~SelfUpdater();

private:
    Ui::SelfUpdater *ui;
};
#endif // MAINWINDOW_H
