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

    int idx = 0;
    QString contributed = "";
    contributed += "Created by\n";
    contributed += "Hazmi Amalul Arifin (hazmi-e205)\n\n";

    contributed += "Contributed projects\n";
    contributed += QString::number(++idx) + ". " + "Qt6" + "\n";
    contributed += "Repository: https://github.com/qt/qt5 \n";
    contributed += "Download: https://www.qt.io/download-open-source \n\n";

    contributed += QString::number(++idx) + ". " + "Premake5" + "\n";
    contributed += "Repository: https://github.com/premake/premake-core \n";
    contributed += "Download: https://github.com/premake/premake-core/releases \n\n";

    QJsonArray dependencies = (*appInfo)->GetJson()["dependencies"].toArray();
    foreach (auto data, dependencies)
    {
        contributed += QString::number(++idx) + ". "+ data.toObject()["project"].toString() + "\n";
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
