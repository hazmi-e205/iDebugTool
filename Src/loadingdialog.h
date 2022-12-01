#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>

namespace Ui {
class LoadingDialog;
}

class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();

    void ShowProgress(QString title, bool withClose = true);
    void SetProgress(int percentage, QString text);

private:
    Ui::LoadingDialog *ui;
};

#endif // LOADINGDIALOG_H
