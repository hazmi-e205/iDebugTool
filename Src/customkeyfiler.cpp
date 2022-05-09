#include "customkeyfiler.h"
#include <QEvent>

CustomKeyFilter::CustomKeyFilter() :
    m_released(false)
{
}

bool CustomKeyFilter::eventFilter(QObject *obj, QEvent *event)
{
    switch(event->type())
    {
    case QEvent::MouseButtonRelease:
        emit pressed(obj);
        return true;
        break;

    case QEvent::KeyRelease:
        emit keyReleased(obj, static_cast<QKeyEvent*>(event));
        return true;
        break;

    default:
        // standard event processing
        return QObject::eventFilter(obj, event);
        break;
    }
}
