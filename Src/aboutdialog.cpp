#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include <QJsonArray>
#include <QJsonObject>

AboutDialog::AboutDialog(AppInfo **appInfo, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    ui->name_label->setText((*appInfo)->GetName());
    ui->version_label->setText("v" + (*appInfo)->GetVersion());

    QString contributed = "";
    contributed += "Created by\n";
    contributed += "Hazmi Amalul Arifin (hazmi-e205)\n\n";

    contributed += "Contributed projects\n";
    QJsonArray dependencies = (*appInfo)->GetJson()["dependencies"].toArray();
    int idx = 0;
    foreach (auto data, dependencies)
    {
        idx += 1;
        contributed += QString::number(idx) + ". "+ data.toObject()["project"].toString() + "\n";
        contributed += "Repository: " + data.toObject()["url"].toString() + "\n";
        if (data.toObject().contains("revision"))
            contributed += "Revision: " + data.toObject()["revision"].toString() + "\n";
        else {
            contributed += "Tag: " + data.toObject()["branch"].toString() + "\n";
        }
        contributed += "\n";
    }
    ui->credits_text->setPlainText(contributed);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
