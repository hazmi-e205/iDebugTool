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
        QString PrivateKey;
        QString PrivateKeyPassword;
        QString Provision;
        bool DoUnpack;
        bool DoRepack;
        bool DoCodesign;
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
        FAILED
    };
    Q_ENUM(SigningStatus);

signals:
    void SigningResult(Recodesigner::SigningStatus status, QString messages);
};

#endif // RECODESIGNER_H
