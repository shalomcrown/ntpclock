#pragma once

#include <QObject>
#include <QtGlobal>

//================================================================================
// Runs SNTP queries. Lives on its own thread; syncNow must not touch widgets.
class NtpWorker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

public slots:
//================================================================================
    void syncNow();

signals:
//================================================================================
    void syncFinished(bool ok, qint64 offsetUs, qint64 ntpUs, const QString& server, const QString& error);
};
