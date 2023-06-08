#include "recodesigner.h"
#include "asyncmanager.h"
#include "utils.h"
#include "openssl.h"
#include "common/common.h"
#include "bundle.h"
#include "extended_zip.h"
#include <iostream>
#include <fstream>
#include <QDir>
#include <QDirIterator>

Recodesigner *Recodesigner::m_instance = nullptr;
Recodesigner *Recodesigner::Get()
{
    if(!m_instance)
        m_instance = new Recodesigner();
    return m_instance;
}

void Recodesigner::Destroy()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

Recodesigner::Recodesigner()
{
    ZLog::g_callback = [&](std::string messages){
        emit SigningResult(SigningStatus::PROCESS, QString::fromStdString(messages).trimmed());
    };
}

void Recodesigner::Process(const Recodesigner::Params& params)
{
    AsyncManager::Get()->StartAsyncRequest([&, params](){
        // Check paths
        string strPath = params.OriginalBuild.toStdString();
        if (!IsFileExists(strPath.c_str()))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Invalid file path: " + params.OriginalBuild + "!");
            return;
        }

        emit SigningResult(SigningStatus::PROCESS, "Loading the certificate and provision...");
        ZSignAsset zSignAsset;
        if (!zSignAsset.Init("", params.PrivateKey.toStdString(), params.Provision.toStdString(), params.NewEntitlements.toStdString(), params.PrivateKeyPassword.toStdString()))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Load certificate and provision failed!");
            return;
        }

        // Local Callback
        auto zipper_callback = [&](int progress, int total, QString messages){
            emit SigningResult(SigningStatus::PROCESS, QString("(%1/%2) %3").arg(progress).arg(total).arg(messages));
        };

        // Unpack build...
        QFileInfo build_info(params.OriginalBuild);
        QString extract_dir = GetDirectory(DIRECTORY_TYPE::ZSIGN_TEMP);
        if (build_info.isDir() && params.OriginalBuild.endsWith(".app", Qt::CaseInsensitive))
        {
            emit SigningResult(SigningStatus::PROCESS, "Copying the build files...");
            QDir dir(extract_dir);
            if (dir.exists())
                dir.removeRecursively();
            QString tempdir = QString("%1/Payload/%2").arg(extract_dir).arg(build_info.fileName());
            if (!CopyFolder(params.OriginalBuild, tempdir, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Copy to temporary dir failed!");
                return;
            }
        }
        else if (params.DoUnpack)
        {
            emit SigningResult(SigningStatus::PROCESS, "Extracting the build file...");
            QDir dir(extract_dir);
            if (dir.exists())
                dir.removeRecursively();
            if (!ZipExtractAll(params.OriginalBuild, extract_dir, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Unpack failed!");
                return;
            }
        }

        // Codesigning...
        if (params.DoCodesign)
        {
            emit SigningResult(SigningStatus::PROCESS, "Re-codesign-ing...");
            ZAppBundle bundle;
            if(!bundle.SignFolder(&zSignAsset,
                                  extract_dir.toStdString(),
                                  params.NewBundleId.toStdString(),
                                  params.NewBundleVersion.toStdString(),
                                  params.NewDisplayName.toStdString(), "", true, false, false))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Sign failed!");
                return;
            }
        }

        // Repack build...
        QString final_build = !params.OutputBuild.isEmpty() ? params.OutputBuild : QString("%1/%2_Recodesigned_%3.ipa")
                .arg(GetDirectory(DIRECTORY_TYPE::RECODESIGNED))
                .arg(build_info.baseName())
                .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
        if (params.DoRepack)
        {
            emit SigningResult(SigningStatus::PROCESS, "Repacking...");
            if (!zip_directory(extract_dir, final_build, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Repack failed!");
                return;
            }
        }

        QString end_message = QString("\nRecodesigned build can be found at\n%1")
                .arg(params.DoRepack ? final_build : (extract_dir + " (Temporary Directory)"));
        if (params.DoInstall)
            emit SigningResult(SigningStatus::INSTALL, QString("Done and continue to install!%1").arg(end_message));
        else
            emit SigningResult(SigningStatus::SUCCESS, QString("Done!%1").arg(end_message));
    });
}
