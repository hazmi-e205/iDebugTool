#include "textviewer.h"
#include "ui_textviewer.h"

TextViewer::TextViewer(QWidget *parent) :
    QDialog(parent),
    m_callback(nullptr),
    ui(new Ui::TextViewer)
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModality::WindowModal);
    connect(ui->okButton, SIGNAL(pressed()), this, SLOT(OnOkPressed()));
}

TextViewer::~TextViewer()
{
    delete ui;
}

void TextViewer::ShowText(QString title, QString text, const std::function<void(QString)>& textCallback)
{
    m_callback = textCallback;
    ui->textEdit->setReadOnly(!textCallback);
    ui->textEdit->setText(text);
    setWindowTitle(title);
    show();
}

void TextViewer::OnOkPressed()
{
    if (m_callback)
        m_callback(ui->textEdit->toPlainText());

    close();
}
