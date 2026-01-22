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
    case QEvent::MouseButtonPress:
        emit pressed(obj);
        break;

    case QEvent::KeyRelease:
        emit keyReleased(obj, static_cast<QKeyEvent*>(event));
        break;

    default:
        // standard event processing
        break;
    }
    return QObject::eventFilter(obj, event);
}
