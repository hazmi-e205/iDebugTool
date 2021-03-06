#ifndef CUSTOMKEYFILER_H
#define CUSTOMKEYFILER_H

#include <QObject>
#include <QKeyEvent>

class CustomKeyFilter : public QObject
{
    Q_OBJECT
public:
    CustomKeyFilter();

private:
    bool m_released;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void pressed(QObject *obj);
    void keyReleased(QObject *obj, QKeyEvent *event);
};

#endif // CUSTOMKEYFILER_H
