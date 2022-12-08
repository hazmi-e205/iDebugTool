#ifndef CUSTOM_GRID_H
#define CUSTOM_GRID_H

#include <QicsTableGrid.h>
#include <QicsTable.h>

class QMenu;
class QMouseEvent;
class CustomGrid : public QicsTableGrid
{
    Q_OBJECT
public:
    CustomGrid(QWidget *w, QicsGridInfo &info, int top_row = 0, int left_column = 0);
    static QicsTableGrid *createGrid(QWidget *w, QicsGridInfo &info, int top_row = 0, int left_column = 0);

protected:
    virtual void handleMousePressEvent(const QicsICell &cell, QMouseEvent *m);
    virtual bool event( QEvent *e );

private:
    QMenu *m_menu;
    QicsTable *m_table;
};

#endif //CUSTOM_GRID_H


