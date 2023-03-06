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

void Recodesigner::Process(QString p12_file, QString p12_password, QString provision_file, QString build)
{
    AsyncManager::Get()->StartAsyncRequest([&, p12_file, p12_password, provision_file, build](){
        // Check paths
        string strPath = build.toStdString();
        if (!IsFileExists(strPath.c_str()))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Invalid file path: " + build + "!");
            return;
        }

        emit SigningResult(SigningStatus::PROCESS, "Loading the certificate and provision...");
        ZSignAsset zSignAsset;
        if (!zSignAsset.Init("", p12_file.toStdString(), provision_file.toStdString(), "", p12_password.toStdString()))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Load certificate and provision failed!");
            return;
        }

        // Unpack build...
        emit SigningResult(SigningStatus::PROCESS, "Extracting the build file...");
        QString extract_dir = GetDirectory(DIRECTORY_TYPE::ZSIGN_TEMP);
        QDir dir(extract_dir);
        if (dir.exists())
            dir.removeRecursively();
        QDir().mkpath(extract_dir);
        auto zipper_callback = [&](int progress, int total, QString messages){
            emit SigningResult(SigningStatus::PROCESS, QString::asprintf("(%d/%d) %s", progress, total, messages.toUtf8().data()));
        };
        if (!zip_extract_all(build, extract_dir, zipper_callback))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Unpack failed!");
            return;
        }

        // Codesigning...
        emit SigningResult(SigningStatus::PROCESS, "Re-codesign-ing...");
        ZAppBundle bundle;
        if(!bundle.SignFolder(&zSignAsset, extract_dir.toStdString(), "", "", "", "", true, false, false))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Sign failed!");
            return;
        }

        // Repack build...
        emit SigningResult(SigningStatus::PROCESS, "Repacking...");
        QFileInfo build_info(build);
        QString final_build = GetDirectory(DIRECTORY_TYPE::RECODESIGNED) + "/" + build_info.baseName() + "_Recodesigned." + build_info.completeSuffix();
        if (!zip_directory(extract_dir, final_build, zipper_callback))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: Repack failed!");
            return;
        }
        emit SigningResult(SigningStatus::SUCCESS, "Done!");
    });
}
