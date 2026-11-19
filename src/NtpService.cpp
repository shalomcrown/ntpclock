#include "NtpService.h"

#include "NtpWorker.h"
#include "PowerEvents.h"

#include <QMutexLocker>

//================================================================================
NtpService::NtpService(QObject* parent) : QObject(parent) {
    m_worker = new NtpWorker();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &NtpWorker::syncFinished, this, &NtpService::onSyncFinished);
    connect(&m_periodic, &QTimer::timeout, this, &NtpService::requestSync);
    m_periodic.setInterval(5 * 60 * 1000);
    m_power = new PowerEvents(this);
    connect(m_power, &PowerEvents::resumed, this, &NtpService::onResume);
}

//================================================================================
NtpService::~NtpService() {
    m_periodic.stop();
    disconnect(m_worker, nullptr, this, nullptr);
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(15000);
        m_worker = nullptr;
        return;
    }
    delete m_worker;
    m_worker = nullptr;
}

//================================================================================
void NtpService::start() {
    if (m_thread.isRunning() == false) {
        m_thread.start();
    }
    {
        QMutexLocker lock(&m_mutex);
        m_snapshot.networkAllowed = true;
    }
    if (m_started == false) {
        m_started = true;
        m_periodic.start();
    }
    requestSync();
}

//================================================================================
ClockSnapshot NtpService::snapshot() const {
    QMutexLocker lock(&m_mutex);
    return m_snapshot;
}

//================================================================================
void NtpService::requestSync() {
    if (m_started == false || m_worker == nullptr) {
        return;
    }
    if (m_busy) {
        m_again = true;
        return;
    }
    m_busy = true;
    QMetaObject::invokeMethod(m_worker, &NtpWorker::syncNow);
}

//================================================================================
void NtpService::onResume() {
    requestSync();
}

//================================================================================
void NtpService::onSyncFinished(bool ok, qint64 offsetUs, qint64 ntpUs, const QString& server, const QString& error) {
    {
        QMutexLocker lock(&m_mutex);
        if (ok) {
            m_snapshot.synced = true;
            m_snapshot.offsetUs = offsetUs;
            m_snapshot.lastNtpUs = ntpUs;
            m_snapshot.server = server;
            m_snapshot.error.clear();
        } else {
            m_snapshot.error = error;
        }
    }
    m_busy = false;
    if (m_again) {
        m_again = false;
        requestSync();
    }
}
