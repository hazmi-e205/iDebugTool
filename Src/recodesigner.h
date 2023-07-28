#ifndef RECODESIGNER_H
#define RECODESIGNER_H

#include <QObject>

class Recodesigner : public QObject
{
    Q_OBJECT
public:
    static Recodesigner *Get();
    static void Destroy();

    Recodesigner();
    struct Params{
    public:
        QString OriginalBuild;
        QString OutputBuild;
        QString PrivateKey;
        QString PrivateKeyPassword;
        QString Provision;
        QString ProvisionExt1;
        QString ProvisionExt2;
        QString NewBundleId;
        QString NewBundleVersion;
        QString NewDisplayName;
        QString NewEntitlements;
        bool DoUnpack;
        bool DoRepack;
        bool DoCodesign;
        bool DoInstall;
    };
    void Process(const Recodesigner::Params& params);

private:
    static Recodesigner *m_instance;

public:
    enum class SigningStatus
    {
        IDLE,
        PROCESS,
        SUCCESS,
        INSTALL,
        FAILED
    };
    Q_ENUM(SigningStatus);

signals:
    void SigningResult(Recodesigner::SigningStatus status, QString messages);
};

#endif // RECODESIGNER_H
