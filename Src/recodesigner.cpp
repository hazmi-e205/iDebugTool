#include "recodesigner.h"
#include "asyncmanager.h"
#include "utils.h"
#include "openssl.h"
#include "common/common.h"
#include "bundle.h"
#include "unzipper.h"
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
        if (!zSignAsset.Init("", params.PrivateKey.toStdString(), params.Provision.toStdString(), "", params.PrivateKeyPassword.toStdString()))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Load certificate and provision failed!");
            return;
        }

        // Local Callback
        auto zipper_callback = [&](int progress, int total, QString messages){
            emit SigningResult(SigningStatus::PROCESS, QString::asprintf("(%d/%d) %s", progress, total, messages.toUtf8().data()));
        };

        // Unpack build...
        QString extract_dir = GetDirectory(DIRECTORY_TYPE::ZSIGN_TEMP);
        if (params.DoUnpack)
        {
            emit SigningResult(SigningStatus::PROCESS, "Extracting the build file...");
            QDir dir(extract_dir);
            if (dir.exists())
                dir.removeRecursively();
            QDir().mkpath(extract_dir);
            if (!zip_extract_all(params.OriginalBuild, extract_dir, zipper_callback))
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
            if(!bundle.SignFolder(&zSignAsset, extract_dir.toStdString(), "", "", "", "", true, false, false))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Sign failed!");
                return;
            }
        }

        // Repack build...
        if (params.DoRepack)
        {
            emit SigningResult(SigningStatus::PROCESS, "Repacking...");
            QFileInfo build_info(params.OriginalBuild);
            QString final_build = GetDirectory(DIRECTORY_TYPE::RECODESIGNED) + "/" + build_info.baseName() + "_Recodesigned." + build_info.completeSuffix();
            if (!zip_directory(extract_dir, final_build, zipper_callback))
            {
                emit SigningResult(SigningStatus::FAILED, "ERROR: Repack failed!");
                return;
            }
        }
        emit SigningResult(SigningStatus::SUCCESS, "Done!");
    });
}
