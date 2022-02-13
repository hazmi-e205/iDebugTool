#include "customkeyfiler.h"
#include <QEvent>
#include <QKeyEvent>

CustomKeyFiler::CustomKeyFiler() :
    m_released(false)
{
}

bool CustomKeyFiler::eventFilter(QObject *obj, QEvent *event)
{
    switch(event->type())
    {
        case QEvent::MouseButtonRelease:
            emit pressed();
            return true;
            break;

        default:
            // standard event processing
            return QObject::eventFilter(obj, event);
            break;
    }
}
