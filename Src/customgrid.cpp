#include "customgrid.h"
#include <QMouseEvent>
#include <QMenu>

CustomGrid::CustomGrid(QWidget *w, QicsGridInfo &info, int top_row, int left_column)
    : QicsTableGrid(w, info, top_row, left_column)
    , m_table((QicsTable*)w)
{
    m_menu = new QMenu(this);
    m_menu->addAction(tr("&Copy"), QKeySequence("Ctrl+C"), w, SLOT(copy()));
    m_menu->addAction(tr("Select &All"), QKeySequence("Ctrl+A"), w, SLOT(selectAll()));
}

QicsTableGrid *CustomGrid::createGrid(QWidget *w, QicsGridInfo &info, int top_row, int left_column)
{
    return (new CustomGrid(w, info, top_row, left_column));
}

void CustomGrid::handleMousePressEvent(const QicsICell &cell, QMouseEvent *m)
{
    if (m->button() == Qt::RightButton)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_menu->popup(m->globalPos());
#else
        m_menu->popup(m->globalPosition().toPoint());
#endif
    else
        QicsTableGrid::handleMousePressEvent(cell, m);
}

bool CustomGrid::event(QEvent *e)
{
    auto keyEvent = static_cast<QKeyEvent*>(e);
    if (keyEvent && keyEvent->matches(QKeySequence::Copy))
    {
        m_table->copy();
        return true;
    }

    if (keyEvent && keyEvent->matches(QKeySequence::SelectAll))
    {
        m_table->selectAll();
        return true;
    }
    return QicsTableGrid::event(e);
}


