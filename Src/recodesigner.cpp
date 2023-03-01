#include "recodesigner.h"
#include "asyncmanager.h"
#include "utils.h"
#include "openssl.h"
#include "common/common.h"
#include "macho.h"
#include "bundle.h"
#include <zip.h>

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
        // chank paths
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
            emit SigningResult(SigningStatus::FAILED, "ERROR: load certificate and provision failed!");
            return;
        }

        // Unpack build...
        emit SigningResult(SigningStatus::PROCESS, "Opening the build file...");
        int errp = 0;
        zip *zf = zip_open(build.toUtf8().data(), 0, &errp);
        if (!zf) {
            emit SigningResult(SigningStatus::FAILED, "ERROR: zip_open: " + build + ": " + QString::number(errp));
            return;
        }

        emit SigningResult(SigningStatus::PROCESS, "Extracting the build file...");
        string strFolder = GetDirectory(DIRECTORY_TYPE::RECODESIGNED).toStdString();
        RemoveFolder(strFolder.c_str());
        if (!zip_extract_all(zf, GetDirectory(DIRECTORY_TYPE::RECODESIGNED))) {
            emit SigningResult(SigningStatus::FAILED, "ERROR: extraction failed!");
            return;
        }

        // Codesigning...
        emit SigningResult(SigningStatus::PROCESS, "Re-codesign-ing...");
        ZAppBundle bundle;
        if(!bundle.SignFolder(&zSignAsset, strFolder, "", "", "", "", true, false, false))
        {
            emit SigningResult(SigningStatus::FAILED, "ERROR: sign failed!");
            return;
        }

        // Repack build...
        emit SigningResult(SigningStatus::SUCCESS, "Success!");
    });
}
