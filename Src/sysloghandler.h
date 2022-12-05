#ifndef SYSLOGHANDLER_H
#define SYSLOGHANDLER_H

#include <QicsTable.h>
#include <QicsDataModelDefault.h>

class SyslogHandler
{
public:
    SyslogHandler(QicsTable **table);

private:
    QicsDataModelDefault *m_dataModel;
    QicsTable *m_table;
};

#endif // SYSLOGHANDLER_H
