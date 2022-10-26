#include "crashsymbolicator.h"
#include <QDebug>
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

    CMachOW* crashed_macho = m_fat->GetMachO(m_crashlog->GetUUID());
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
                            symbol = QString::fromWCharArray(m_fat->GetSymbolNameAt(m_crashlog->GetUUID(), addr64).c_str());
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
                            symbol = QString::fromWCharArray(m_fat->GetSymbolNameAt(m_crashlog->GetUUID(), addr64 - m_crashlog->Start()).c_str());
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

void CrashSymbolicator::onFind(CMachOCrashLog &crashlog)
{
    m_crashlog = new CMachOCrashLogW(crashlog);
}

void CrashSymbolicator::onFind(CMachODSym &dsym)
{
    m_fat = new CMachODSymW(dsym);
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
