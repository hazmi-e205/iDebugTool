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

void TextViewer::ShowText(QString title, QString initialText, bool readOnly, const std::function<void(QString)>& textCallback, QString customButton)
{
    m_callback = textCallback;
    ui->textEdit->setReadOnly(readOnly);
    ui->textEdit->setText(initialText);
    ui->okButton->setText(customButton);
    setWindowTitle(title);
    show();
}

void TextViewer::AppendText(QString text)
{
    ui->textEdit->append(text);
}

void TextViewer::OnOkPressed()
{
    if (m_callback)
        m_callback(ui->textEdit->toPlainText());
    close();
}
