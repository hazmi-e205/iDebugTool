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
#include <QObject>

using namespace MachO;

class CrashSymbolicator : public QObject, public CSearchMachO::ISender
{
    Q_OBJECT
public:
    static CrashSymbolicator *Get();
    static void Destroy();
    CrashSymbolicator();
    void Process(QString crashlogPath, QString dsymDir);
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

signals:
    void SymbolicateResult(QString messages, bool error = false);
};

#endif // CRASHSYMBOLICATOR_H
