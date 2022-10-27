#include "crashsymbolicator.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <fstream>
#include "utility/CDirectory.h"

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
{
}

QString CrashSymbolicator::Proccess(QString crashlogPath, QString dsymDir)
{
    QString out;

    QString oldstyle = ConvertToOldStyle(crashlogPath);
    if (!oldstyle.isEmpty())
    {
        return Proccess(oldstyle, dsymDir);
    }

    bool result = CSearchMachO::Search(crashlogPath.toStdWString().c_str(), *this);
    qDebug() << "crashlog: " << result;
    if (!result) return out;
    result = CSearchMachO::Search(dsymDir.toStdWString().c_str(), *this);
    qDebug() << "dsym: " << result;
    if (!result) return out;

    bool inlining = false;
    std::map<std::wstring, std::wstring> linkNameOfLibWithUUID = m_crashlog->GetLinkMap();
    QString line = "";
    std::wstring name = m_dsym->GetName();

    CMachOW* crashed_macho = m_dsym->GetMachO(m_crashlog->GetUUID());
    std::ifstream infile(crashlogPath.toStdString());
    std::string cline;
    while (std::getline(infile, cline))
    {
        line = QString(cline.c_str());
        if (!line.isNull())
        {
            if (IsUnsymbolicatedLine(line))
            {
                if (line.indexOf(name) >= 0)
                {
                    int replace_start = (line.indexOf(kSPACE_TAB_SEPARATOR) + 2);
                    replace_start = (line.indexOf(kSPACE, replace_start) + 1);
                    int replace_end = line.length();
                    int start = line.indexOf(kPLUS_SPACE_SEPARATOR);

                    if (replace_start >= 0 && start > replace_start && replace_end > start)
                    {
                        QString address = line.mid(start + 2, (replace_end - start) - 2);
                        uint64_t addr64 = address.toULongLong();
                        QString symbol = "<invalid>";
                        if (inlining)
                        {
                            symbol = QString::fromWCharArray(crashed_macho->GetInliningInfo(addr64).c_str());
                        }
                        else
                        {
                            symbol = QString::fromWCharArray(m_dsym->GetSymbolNameAt(m_crashlog->GetUUID(), addr64).c_str());
                        }
                        line = line.mid(0, replace_start) + " " + symbol + " " + line.mid(replace_end);
                    }
                }
                else
                {
                    for (const auto& linkname : linkNameOfLibWithUUID)
                    {
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
                        if(GetSystemSymbols() != nullptr)
                        {
                            //CMachOW* macho = m_dsym->GetMachO(m_crashlog->GetUUID());
                            QString systemSymbol = QString::fromWCharArray(GetSystemSymbols()->GetSymbolNameAt(linkname.second, addr64).c_str());
                            if (!(systemSymbol.isNull() || systemSymbol.isEmpty()))
                            {
                                line = line.mid(0, replace_start) + " " + systemSymbol + " " + line.mid(replace_end);
                            }
                        }
                    }
                }
            }
            else if (IsExceptionBacktrace(line))
            {
                QString result = "";
                QString addresses_str = line.mid(1, line.length() - 2);
                QStringList addresses = addresses_str.split(kSPACE);
                for (int i = 0; i < addresses.length(); ++i)
                {
                    uint64_t addr64 = addresses[i].toULongLong(nullptr, 16);
                    QString symbol = "<unresolved>", image = "<unresolved>", _template = "";

                    if (addr64 >= m_crashlog->Start() && addr64 < m_crashlog->End())
                    {
                        if (inlining)
                        {
                            symbol = QString::fromWCharArray(crashed_macho->GetInliningInfo(addr64 - m_crashlog->Start()).c_str());
                        }
                        else
                        {
                            symbol = QString::fromWCharArray(m_dsym->GetSymbolNameAt(m_crashlog->GetUUID(), addr64 - m_crashlog->Start()).c_str());
                        }
                        QString hexvalue = QString("%1").arg(addr64, 8, 16, QLatin1Char( '0' ));
                        _template = QString("%1%2\t0x%3 %4").arg(i, -4).arg(QString::fromWCharArray(m_crashlog->GetImageName().c_str())).arg(hexvalue).arg(symbol);//String.Format("{0,-4:d}{1,-30:s}\t0x{2:x} {3:s}", i, m_crashlog->GetImageName(), addr64, symbol);
                    }
                    else
                    {
                        QString hexvalue = QString("%1").arg(addr64, 8, 16, QLatin1Char( '0' ));
                        _template = QString("%1%2\t0x%3 %4").arg(i, -4).arg(image).arg(hexvalue).arg(symbol);//String.Format("{0,-4:d}{1,-30:s}\t0x{2:x} {3:s}", i, image, addr64, symbol);
                    }
                    result += _template + "\n";
                }
                line = result;
            }
        }
        out.append(line + "\n");
    }
    crashed_macho->CleanUpInliningInfo();
    return out;
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
        content.append(QString("%1").arg(binaryImage["name"].toString(""), -40));

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
