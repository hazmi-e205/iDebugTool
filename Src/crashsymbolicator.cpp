#include "crashsymbolicator.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <fstream>
#include "utility/CDirectory.h"
#include "utils.h"
#include "CMachOW.h"

const QString kSPACE_TAB_SEPARATOR = " \t";
const QString kPLUS_SPACE_SEPARATOR = "+ ";
const char kSPACE = ' ';

CrashSymbolicator *CrashSymbolicator::m_instance = nullptr;
CrashSymbolicator *CrashSymbolicator::Get()
{
    if(!m_instance)
        m_instance = new CrashSymbolicator();
    return m_instance;
}

void CrashSymbolicator::Destroy()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

CrashSymbolicator::CrashSymbolicator()
    : m_thread(new QThread())
{
    connect(m_thread, SIGNAL(started()), SLOT(doWork()));
    moveToThread(m_thread);
}

void CrashSymbolicator::Process(QString crashlogPath, QString dsymDir)
{
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    m_crashlogPath = crashlogPath;
    m_dsymPath = dsymDir;
    m_thread->start();
}

void CrashSymbolicator::doWork()
{
    SymbolicatedData data;
    QString out;
    QString oldstyle = ConvertToOldStyle(m_crashlogPath);
    if (!oldstyle.isEmpty())
    {
        m_crashlogPath = oldstyle;
    }

    bool result = CSearchMachO::Search(m_crashlogPath.toStdWString().c_str(), *this);
    if (!result)
    {
        data.rawString = "Crashlog not found!";
        emit SymbolicateResult2(100, data, true);
        m_thread->quit();
        m_thread->wait();
        return;
    }

    if (QFileInfo(m_dsymPath).isFile())
    {
        m_dsym = new CMachODSymW(CMachODSym(m_dsymPath.toStdWString(), m_dsymPath.toStdWString()));
        result = true;
    }
    else
    {
        result = CSearchMachO::Search(m_dsymPath.toStdWString().c_str(), *this);
    }
    if (!result)
    {
        data.rawString = "Crashlog not found!";
        emit SymbolicateResult2(100, data, true);
        m_thread->quit();
        m_thread->wait();
        return;
    }

    bool inlining = false;
    std::map<std::wstring, std::wstring> linkNameOfLibWithUUID = m_crashlog->GetLinkMap();
    QString line = "";
    QString name = QString::fromStdWString(m_dsym->GetName());

    CMachOW* crashed_macho = NULL;
    try {
        crashed_macho = m_dsym->GetMachO(m_crashlog->GetUUID());
    } catch (const char* messages) {
        data.rawString = QString("Crashlog invalid! (%1)\n"
                                 "Please check your crashlog and dsym or try to symbolicate or debug it in xcode...\n"
                                 "\nLast crashlog: %2\n"
                                 "\nLast dsym: %3\n").arg(messages).arg(m_crashlogPath).arg(m_dsymPath);
        emit SymbolicateResult2(100, data, true);
        m_thread->quit();
        m_thread->wait();
        return;
    }
    std::ifstream infile(m_crashlogPath.toStdString());
    quint64 lineTotal = std::count(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>(), '\n');
    quint64 lineCount = 0;
    std::string cline;
    StackTrace stacktrace;
    infile.seekg(0);
    while (std::getline(infile, cline))
    {
        line = QString(cline.c_str());
        if (!line.isNull())
        {
            QString threadname = FindRegex(line, "Thread [0-9]+ name:");
            if (!threadname.isEmpty())
                threadname = line.trimmed();
            else
                threadname = FindRegex(line, "Thread [0-9]+");

            if (!threadname.isEmpty() && !stacktrace.threadName.contains(threadname, Qt::CaseInsensitive))
            {
                if (!stacktrace.threadName.isEmpty() && !stacktrace.threadName.contains(threadname, Qt::CaseInsensitive))
                    data.stackTraces.append(stacktrace);

                stacktrace = StackTrace();
                stacktrace.threadName = threadname;
            }

            if (IsUnsymbolicatedLine(line))
            {
                StackLine stackline;
                QString symbol = "<invalid>";

                uint64_t addr64 = 0;
                int replace_start = (line.indexOf(kSPACE_TAB_SEPARATOR) + 2);
                replace_start = (line.indexOf(kSPACE, replace_start) + 1);
                int replace_end = line.length();
                int start = line.indexOf(kPLUS_SPACE_SEPARATOR);
                if (replace_start >= 0 && start > replace_start && replace_end > start)
                {
                    QString address = line.mid(start + 2, (replace_end - start) - 2);
                    addr64 = address.toULongLong();
                }

                if (line.indexOf(name) >= 0)
                {
                    if (inlining)
                    {
                        symbol = QString::fromStdWString(crashed_macho->GetInliningInfo(addr64));
                    }
                    else
                    {
                        symbol = QString::fromStdWString(m_dsym->GetSymbolNameAt(m_crashlog->GetUUID(), addr64));
                    }
                    line = line.mid(0, replace_start) + " " + symbol + " " + line.mid(replace_end);
                    stackline.binary = name;
                }
                else
                {
                    stackline.binary = line.split(QRegularExpression("\\s+")).at(1);
                    symbol = line.mid(replace_start);
                    for (const auto& linkname : linkNameOfLibWithUUID)
                    {
                        if(GetSystemSymbols() != nullptr)
                        {
                            //CMachOW* macho = m_dsym->GetMachO(m_crashlog->GetUUID());
                            QString systemSymbol = QString::fromStdWString(GetSystemSymbols()->GetSymbolNameAt(linkname.second, addr64));
                            if (!(systemSymbol.isNull() || systemSymbol.isEmpty()))
                            {
                                line = line.mid(0, replace_start) + " " + systemSymbol + " " + line.mid(replace_end);
                                stackline.binary = QString::fromStdWString(linkname.first);
                                symbol = systemSymbol;
                            }
                        }
                    }
                }

                QString symbol_line = FindRegex(symbol, "\\([\\S]*:[0-9]+\\)");
                if (symbol_line.isEmpty())
                {
                    stackline.function = symbol;
                }
                else
                {
                    stackline.line = symbol_line.remove("(").remove(")");
                    stackline.function = symbol.remove(symbol_line).trimmed();
                }
                stacktrace.lines.append(stackline);
            }
            else if (IsExceptionBacktrace(line))
            {
                stacktrace = StackTrace();
                stacktrace.threadName = "Last Exception Backtrace";

                QString result = "";
                QString addresses_str = line.mid(1, line.length() - 2);
                QStringList addresses = addresses_str.split(kSPACE);
                for (int i = 0; i < addresses.length(); ++i)
                {
                    StackLine stackline;
                    uint64_t addr64 = addresses[i].toULongLong(nullptr, 16);
                    QString hexvalue = QString("0x%1").arg(addr64, 8, 16, QLatin1Char( '0' ));
                    QString symbol = "<unresolved>", image = "<unresolved>", _template = "";

                    if (addr64 >= m_crashlog->Start() && addr64 < m_crashlog->End())
                    {
                        if (inlining)
                        {
                            symbol = QString::fromStdWString(crashed_macho->GetInliningInfo(addr64 - m_crashlog->Start()));
                        }
                        else
                        {
                            symbol = QString::fromStdWString(m_dsym->GetSymbolNameAt(m_crashlog->GetUUID(), addr64 - m_crashlog->Start()));
                        }
                        image = QString::fromStdWString(m_crashlog->GetImageName());
                    }
                    _template = QString("%1%2\t%3 %4").arg(i, -4).arg(image).arg(hexvalue).arg(symbol);
                    result += _template + "\n";

                    stackline.binary = image;
                    QString symbol_line = FindRegex(symbol, "\\([\\S]*:[0-9]+\\)");
                    if (symbol_line.isEmpty())
                    {
                        stackline.line = symbol;
                    }
                    else
                    {
                        stackline.line = symbol_line.remove("(").remove(")");
                        stackline.function = symbol.remove(symbol_line).trimmed();
                    }
                    stacktrace.lines.append(stackline);
                }
                line = result;
            }
            else
            {
                int replace_start = (line.indexOf(kSPACE_TAB_SEPARATOR) + 2);
                replace_start = (line.indexOf(kSPACE, replace_start) + 1);
                int replace_end = line.length();
                int start = line.indexOf(kPLUS_SPACE_SEPARATOR);
                if (replace_start >= 0 && start > replace_start && replace_end > start)
                {
                    StackLine stackline;
                    stackline.binary = line.split(QRegularExpression("\\s+")).at(1);
                    stackline.function = line.mid(replace_start);
                    stacktrace.lines.append(stackline);
                }
            }
        }
        out.append(line + "\n");
        lineCount += 1;
        qDebug() << "Percentage: " << (lineCount * 100) / lineTotal;
        emit SymbolicateResult2((lineCount * 100) / lineTotal, SymbolicatedData());
    }
    crashed_macho->CleanUpInliningInfo();
    data.rawString = out;
    emit SymbolicateResult2(100, data);
    m_thread->quit();
    m_thread->wait();
}

QString CrashSymbolicator::ConvertToOldStyle(QString crashlogPath)
{
    QFile inputFile(crashlogPath);
    QString header, content;
    if (inputFile.open(QIODevice::ReadOnly))
    {
       QTextStream in(&inputFile);
       while (!in.atEnd())
       {
           if (header.isEmpty())
           {
               header = in.readLine();
           }
           else
           {
               content += in.readLine() + "\n";
           }
       }
       inputFile.close();
    }

    QJsonParseError error;
    QJsonDocument headerDoc = QJsonDocument::fromJson(header.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) return "";
    QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) return "";

    QFileInfo info(crashlogPath);
    QString filepath = info.absolutePath() + "/" + info.baseName() + "_old." + info.completeSuffix();
    QString oldStyle = header + "\n" + ConvertToOldStyle(headerDoc, contentDoc);
    QFile f(filepath);
    if (f.open(QIODevice::ReadWrite))
    {
        QTextStream stream(&f);
        stream << oldStyle;
        f.close();
        return filepath;
    }
    return "";
}

QString CrashSymbolicator::ConvertToOldStyle(const QJsonDocument& ipsHeader, const QJsonDocument& payload)
{
    QString content = buildHeader(ipsHeader, payload);
    QJsonArray binaryImages = payload["usedImages"].toArray();

    if (payload["threads"].isArray())
    {
        QJsonArray threads = payload["threads"].toArray();
        foreach (auto _thread, threads)
        {
            content.append("\n");
            QJsonObject thread = _thread.toObject();

            if (thread["name"].isString() || thread["queue"].isString())
            {
                QString name = thread["name"].isString() ? thread["name"].toString("") : thread["queue"].toString("");
                content.append(QString("Thread %1 name:  %2\n").arg(thread["id"].toInt()).arg(name));
            }

            if (thread["triggered"].isBool() && thread["triggered"].toBool() == false)
                content.append(QString("Thread %1 Crashed:\n").arg(thread["id"].toInt()));
            else
                content.append(QString("Thread %1:\n").arg(thread["id"].toInt()));
            content.append(buildFrameStack(thread["frames"].toArray(), binaryImages));
        }
    }
    content.append(buildBinaryImages(binaryImages));
    return content;
}

QString CrashSymbolicator::buildHeader(const QJsonDocument &ipsHeader, const QJsonDocument &payload)
{
    QString content = "";
    content.append(QString("Incident Identifier: %1\n").arg(ipsHeader["incident_id"].toString("")));
    content.append(QString("CrashReporter Key:   %1\n").arg(payload["crashReporterKey"].toString("")));
    content.append(QString("Hardware Model:      %1\n").arg(payload["modelCode"].toString("")));
    content.append(QString("Process:             %1 [%2]\n").arg(payload["procName"].toString("")).arg(payload["pid"].toInt()));
    content.append(QString("Path:                %1\n").arg(payload["procPath"].toString("")));
    if (payload["bundleInfo"].isObject())
    {
        QJsonObject bundleInfo = payload["bundleInfo"].toObject();
        content.append(QString("Identifier:          %1\n").arg(bundleInfo["CFBundleIdentifier"].toString("")));
        content.append(QString("Version:             %1 (%2)\n").arg(bundleInfo["CFBundleShortVersionString"].toString("")).arg(bundleInfo["CFBundleVersion"].toString("")));
    }
    content.append(QString("Report Version:      104\n"));
    content.append(QString("Code Type:           %1 (Native)\n").arg(payload["cpuType"].toString("")));
    content.append(QString("Role:                %1\n").arg(payload["procRole"].toString("")));
    content.append(QString("Parent Process:      %1 [%2]\n").arg(payload["parentProc"].toString("")).arg(payload["parentPid"].toInt()));
    content.append(QString("Coalition:           %1 [%2]\n").arg(payload["coalitionName"].toString("")).arg(payload["coalitionID"].toInt()));
    content.append(QString("\n"));
    content.append(QString("Date/Time:           %1\n").arg(payload["captureTime"].toString("")));
    content.append(QString("Launch Time:         %1\n").arg(payload["procLaunch"].toString("")));
    content.append(QString("OS Version:          %1\n").arg(ipsHeader["os_version"].toString("")));
    content.append(QString("Release Type:        %1\n").arg(payload["osVersion"]["releaseType"].toString("")));
    content.append(QString("Baseband Version:    %1\n").arg(payload["basebandVersion"].toString("")));
    content.append(QString("\n"));

    QJsonObject exception = payload["exception"].toObject();
    content.append(QString("Exception Type:  %1 (%2)\n").arg(exception["type"].toString("")).arg(exception["signal"].toString("")));
    content.append(QString("Exception Codes: %1\n").arg(exception["codes"].toString("")));
    content.append(QString("Triggered by Thread:  %1\n").arg(payload["faultingThread"].toInt()));
    content.append(QString("\n"));
    return content;
}

QString CrashSymbolicator::buildFrameStack(const QJsonArray &frames, const QJsonArray &binaryImages)
{
    QString content = "";
    int idx = 0;
    foreach (auto _frame, frames)
    {
        QJsonObject frame = _frame.toObject();
        QJsonObject binaryImage = binaryImages[frame["imageIndex"].toInt()].toObject();
        int address = frame["imageOffset"].toInt() + binaryImage["base"].toInt();
        content.append(QString("%1").arg(idx, -5));
        content.append(QString("%1").arg(binaryImage["name"].toString(""), -30));
        content.append("\t");

        QString hexvalue = QString("%1").arg(address, 8, 16, QLatin1Char( '0' ));
        content.append(QString("0x%1 ").arg(hexvalue));

        if (frame["symbol"].isString() && frame["symbolLocation"].isDouble())
        {
            content.append(QString("%1 + %2").arg(frame["symbol"].toString("")).arg(frame["symbolLocation"].toInt()));
        }
        else
        {
            int64_t base = binaryImage["base"].toInteger();
            QString hexbase = QString("%1").arg(base, 8, 16, QLatin1Char( '0' ));
            content.append(QString("0x%1 + %2").arg(hexbase).arg(frame["imageOffset"].toInt()));
        }

        if (frame["sourceFile"].isString() && frame["sourceLine"].isDouble())
        {
            content.append(QString(" (%1:%2)").arg(frame["sourceFile"].toString("")).arg(frame["sourceLine"].toInt()));
        }
        content.append("\n");
        idx++;
    }
    return content;
}

QString CrashSymbolicator::buildBinaryImages(const QJsonArray &binaryImages)
{
    QString content = "\nBinary Images:\n";
    foreach (auto _image, binaryImages)
    {
        QJsonObject image = _image.toObject();
        QString hexbase = QString("%1").arg(image["base"].toInteger(), 8, 16, QLatin1Char( '0' ));
        QString hexbase2 = QString("%1").arg(image["base"].toInteger() + image["size"].toInteger() - 1, 8, 16, QLatin1Char( '0' ));
        content.append(QString("0x%1 - 0x%2 ").arg(hexbase).arg(hexbase2));
        content.append(QString("%1 %2 ").arg(image["name"].toString(""), image["arch"].toString("")));
        content.append(QString("<%1> %2\n").arg(image["uuid"].toString("").replace('-', "")).arg(image["path"].toString("")));
    }
    return content;
}

void CrashSymbolicator::onFind(CMachOCrashLog &crashlog)
{
    m_crashlog = new CMachOCrashLogW(crashlog);
}

void CrashSymbolicator::onFind(CMachODSym &dsym)
{
    m_dsym = new CMachODSymW(dsym);
}

void CrashSymbolicator::onFind(CMachOApplication &app)
{
    qDebug() << __PRETTY_FUNCTION__;
}

void CrashSymbolicator::onFind(CMachODyLib &dylib)
{
    qDebug() << __PRETTY_FUNCTION__;
}

bool CrashSymbolicator::IsExceptionBacktrace(QString line)
{
    return (line.startsWith("(0x") && line.endsWith(")"));
}

bool CrashSymbolicator::IsUnsymbolicatedLine(QString line)
{
    int tab_idx = line.indexOf(kSPACE_TAB_SEPARATOR);
    if (tab_idx < 0) { return false; }
    int first_idx = line.indexOf("0x", tab_idx);
    if (first_idx < 0) { return false; }
    int second_idx = line.indexOf("0x", (first_idx + 2));
    if (second_idx < 0) { return false; }
    int plus_idx = line.indexOf('+', (second_idx + 2));
    if (plus_idx < 0) { return false; }
    return (plus_idx > second_idx
            && second_idx > first_idx
            && first_idx > tab_idx);
}

CSymbolsPackW *CrashSymbolicator::GetSystemSymbols()
{
    CSymbolsPackW* opack = nullptr;
    static const wchar_t kSYSTEM_SYMBOLS_FOLDER[] = L"SystemSymbols";
    std::wstring file(kSYSTEM_SYMBOLS_FOLDER);
    file.append(L"\\package.symbols");
    if(io::CDirectory::IsExistDir(kSYSTEM_SYMBOLS_FOLDER))
    {
        if(io::CDirectory::IsExistFile(file))
        {
            SymbolPackage::Ptr pack(new SymbolPackage(io::source_of_stream(file.c_str(), io::EENCODING_LITTLE_ENDIAN)));
            opack = new CSymbolsPackW(pack);
        }
    }
    return opack;
}
