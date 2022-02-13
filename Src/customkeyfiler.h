#ifndef CUSTOMKEYFILER_H
#define CUSTOMKEYFILER_H

#include <QObject>

class CustomKeyFiler : public QObject
{
    Q_OBJECT
public:
    CustomKeyFiler();

private:
    bool m_released;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void pressed();
};

#endif // CUSTOMKEYFILER_H
