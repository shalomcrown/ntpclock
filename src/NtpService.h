#pragma once

#include "ClockFormat.h"

#include <QObject>
#include <QThread>
#include <QTimer>

#include <QMutex>

class NtpWorker;
class PowerEvents;

//================================================================================
// Owns the NTP thread. The display reads snapshot() and never blocks on the network.
class NtpService : public QObject {
    Q_OBJECT

public:
//================================================================================
    explicit NtpService(QObject* parent = nullptr);
//================================================================================
    ~NtpService() override;

//================================================================================
    void start();
//================================================================================
    ClockSnapshot snapshot() const;

private slots:
//================================================================================
    void onSyncFinished(bool ok, qint64 offsetUs, qint64 ntpUs, const QString& server, const QString& error);
//================================================================================
    void onResume();

private:
//================================================================================
    void requestSync();

    QThread m_thread;
    NtpWorker* m_worker = nullptr;
    PowerEvents* m_power = nullptr;
    QTimer m_periodic;
    mutable QMutex m_mutex;
    ClockSnapshot m_snapshot;
    bool m_started = false;
    bool m_busy = false;
    bool m_again = false;
};
