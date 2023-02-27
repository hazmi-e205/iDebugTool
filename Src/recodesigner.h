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
    void Process(QString p12_file, QString p12_password, QString provision_file, QString build);

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

signals:
    void SigningResult(Recodesigner::SigningStatus status, QString messages);
};

#endif // RECODESIGNER_H
