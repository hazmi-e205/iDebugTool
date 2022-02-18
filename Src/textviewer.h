#ifndef TEXTVIEWER_H
#define TEXTVIEWER_H

#include <QDialog>

namespace Ui {
class TextViewer;
}

class TextViewer : public QDialog
{
    Q_OBJECT

public:
    explicit TextViewer(QWidget *parent = nullptr);
    ~TextViewer();
    void ShowText(QString title, QString text);

private:
    Ui::TextViewer *ui;
};

#endif // TEXTVIEWER_H
