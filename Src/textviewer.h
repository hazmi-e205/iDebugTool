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

    /**
     * Show text viewer dialog with options.
     * @param title of dialog.
     * @param initial text that want to show to text viewer.
     * @param callback if you want to use it as text editor, nullptr mean read only.
     */
    void ShowText(QString title, QString text, const std::function<void(QString)>& textCallback = nullptr);
    void AppendText(QString text);

private:
    std::function<void (QString)> m_callback;
    Ui::TextViewer *ui;

private slots:
    void OnOkPressed();
};

#endif // TEXTVIEWER_H
