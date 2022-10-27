#ifndef CRASHSYMBOLICATOR_H
#define CRASHSYMBOLICATOR_H

#include "utility/CSearchMachO.h"
#include "CMachOCrashLogW.h"
#include "CMachODSymW.h"
#include "CMachOW.h"
#include "CSymbolsPackW.h"
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace MachO;

class CrashSymbolicator : public CSearchMachO::ISender
{
public:
    static CrashSymbolicator *Get();
    static void Destroy();
    CrashSymbolicator();
    QString Proccess(QString crashlogPath, QString dsymDir);
    QString ConvertToOldStyle(QString crashlogPath);

protected:
    void onFind(CMachOCrashLog& crashlog) override;
    void onFind(CMachODSym& dsym) override;
    void onFind(CMachOApplication& app) override;
    void onFind(CMachODyLib& dylib) override;

private:
    bool IsExceptionBacktrace(QString line);
    bool IsUnsymbolicatedLine(QString line);
    CSymbolsPackW* GetSystemSymbols();

    QString ConvertToOldStyle(const QJsonDocument& ipsHeader, const QJsonDocument& payload);
    QString buildHeader(const QJsonDocument& ipsHeader, const QJsonDocument& payload);
    QString buildFrameStack(const QJsonArray& frames, const QJsonArray& binaryImages);
    QString buildBinaryImages(const QJsonArray& binaryImages);

    static CrashSymbolicator *m_instance;
    CMachOCrashLogW* m_crashlog;
    CMachODSymW* m_dsym;
};

#endif // CRASHSYMBOLICATOR_H
