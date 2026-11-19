#include "PowerEvents.h"

#include <QCoreApplication>
#include <QGuiApplication>

#if defined(NTPCLOCK_HAS_DBUS)
#include <QDBusConnection>
#endif

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

//================================================================================
PowerEvents::PowerEvents(QObject* parent) : QObject(parent) {
#if defined(Q_OS_WIN)
    if (qApp != nullptr) {
        qApp->installNativeEventFilter(this);
    }
#endif
#if defined(NTPCLOCK_HAS_DBUS)
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                         QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"),
                                         QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
#endif
    if (auto* gui = qobject_cast<QGuiApplication*>(qApp)) {
        connect(gui, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            const bool asleep = state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden;
            if (m_asleep && state == Qt::ApplicationActive) {
                emit resumed();
            }
            m_asleep = asleep;
        });
    }
}

//================================================================================
PowerEvents::~PowerEvents() {
#if defined(Q_OS_WIN)
    if (qApp != nullptr) {
        qApp->removeNativeEventFilter(this);
    }
#endif
}

//================================================================================
bool PowerEvents::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
#if defined(Q_OS_WIN)
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }
    const auto* msg = static_cast<MSG*>(message);
    if (msg != nullptr && msg->message == WM_POWERBROADCAST &&
        (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND)) {
        emit resumed();
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

//================================================================================
void PowerEvents::onPrepareForSleep(bool sleeping) {
    // logind passes true when the machine is about to sleep, and false on the way back.
    if (sleeping == false) {
        emit resumed();
    }
}
