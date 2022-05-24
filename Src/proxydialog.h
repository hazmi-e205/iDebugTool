#ifndef PROXYDIALOG_H
#define PROXYDIALOG_H

#include <QDialog>

namespace Ui {
class ProxyDialog;
}

class ProxyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProxyDialog(QWidget *parent = nullptr);
    ~ProxyDialog();
    void ShowDialog();
    void UseExisting();

private:
    Ui::ProxyDialog *ui;

private slots:
    void OnCheckClicked();
};

#endif // PROXYDIALOG_H
