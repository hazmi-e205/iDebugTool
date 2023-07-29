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
        emit SigningResult(SigningStatus::PROCESS, 50.f, QString::fromStdString(messages).trimmed());
    };
}

void Recodesigner::Process(const Recodesigner::Params& params)
{
    AsyncManager::Get()->StartAsyncRequest([&, params](){
        // Check paths
        string strPath = params.OriginalBuild.toStdString();
        if (!IsFileExists(strPath.c_str()))
        {
            emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Invalid file path: " + params.OriginalBuild + "!");
            return;
        }

        emit SigningResult(SigningStatus::PROCESS, 0.f, "Loading the certificate and provision...");
        ZSignAsset zSignAsset, zSignAssetExt1, zSignAssetExt2;
        if (!zSignAsset.Init("", params.PrivateKey.toStdString(), params.Provision.toStdString(), params.NewEntitlements.toStdString(), params.PrivateKeyPassword.toStdString()))
        {
            emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Load certificate and provision failed!");
            return;
        }
        if (!params.ProvisionExt1.isEmpty())
        {
            if (!zSignAsset.Init("", params.PrivateKey.toStdString(), params.ProvisionExt1.toStdString(), "", params.PrivateKeyPassword.toStdString()))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Load certificate and provision extension 1 failed!");
                return;
            }
        }
        if (!params.ProvisionExt2.isEmpty())
        {
            if (!zSignAsset.Init("", params.PrivateKey.toStdString(), params.ProvisionExt2.toStdString(), "", params.PrivateKeyPassword.toStdString()))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Load certificate and provision extension 2 failed!");
                return;
            }
        }

        // Local Callback
        bool is_extracting = false;
        auto zipper_callback = [&](float percentage, QString messages){
            emit SigningResult(SigningStatus::PROCESS, (percentage / 2.f) + (is_extracting ? 0.f : 50.f), messages);
        };

        // Unpack build...
        QFileInfo build_info(params.OriginalBuild);
        QString extract_dir = GetDirectory(DIRECTORY_TYPE::ZSIGN_TEMP);
        if (build_info.isDir() && params.OriginalBuild.endsWith(".app", Qt::CaseInsensitive))
        {
            emit SigningResult(SigningStatus::PROCESS, 50.01f, "Copying the build files...");
            QDir dir(extract_dir);
            if (dir.exists())
                dir.removeRecursively();
            QString tempdir = QString("%1/Payload/%2").arg(extract_dir).arg(build_info.fileName());

            auto copy_callback = [&](int progress, int total, QString messages){
                float percentage = ((float)progress * 50.f) / (float)total;
                emit SigningResult(SigningStatus::PROCESS, percentage, QString("(%1/%2) %3").arg(progress).arg(total).arg(messages));
            };
            if (!CopyFolder(params.OriginalBuild, tempdir, copy_callback))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Copy to temporary dir failed!");
                return;
            }
        }
        else if (params.DoUnpack)
        {
            emit SigningResult(SigningStatus::PROCESS, 0.f, "Extracting the build file...");
            QDir dir(extract_dir);
            if (dir.exists())
                dir.removeRecursively();
            is_extracting = true;
            if (!ZipExtractAll(params.OriginalBuild, extract_dir, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Unpack failed!");
                return;
            }
        }

        // Codesigning...
        if (params.DoCodesign)
        {
            emit SigningResult(SigningStatus::PROCESS, 50.f, "Re-codesign-ing...");
            ZAppBundle bundle;
            if(!bundle.SignFolder(&zSignAsset,
                                  extract_dir.toStdString(),
                                  params.NewBundleId.toStdString(),
                                  params.NewBundleVersion.toStdString(),
                                  params.NewDisplayName.toStdString(), "", true, false, false))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Sign failed!");
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
            emit SigningResult(SigningStatus::PROCESS, 50.01f, "Repacking...");
            is_extracting = false;
            if (!ZipDirectory(extract_dir, final_build, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, 100.f, "ERROR: Repack failed!");
                return;
            }
        }

        QString end_message = QString("\nRecodesigned build can be found at\n%1")
                .arg(params.DoRepack ? final_build : (extract_dir + " (Temporary Directory)"));
        if (params.DoInstall)
            emit SigningResult(SigningStatus::INSTALL, 100.f, QString("Done and continue to install!%1").arg(end_message));
        else
            emit SigningResult(SigningStatus::SUCCESS, 100.f, QString("Done!%1").arg(end_message));
    });
}
