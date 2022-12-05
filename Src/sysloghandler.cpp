#include "sysloghandler.h"
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>


SyslogHandler::SyslogHandler(QicsTable **table)
    : m_table(*table)
{
}
