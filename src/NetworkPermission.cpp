#include "NetworkPermission.h"

#include <QSettings>

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {
bool operatingSystemAllowsNetwork() {
#if defined(Q_OS_ANDROID)
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid() == false) {
        return true;
    }
    const jint result = context.callMethod<jint>(
            "checkSelfPermission", "(Ljava/lang/String;)I",
            QJniObject::fromString(QStringLiteral("android.permission.INTERNET")).object<jstring>());
    QJniEnvironment env;
    if (env.checkAndClearExceptions()) {
        return true;
    }
    return result == 0;
#else
    return true;
#endif
}
}  // namespace

NetworkPermission::Status NetworkPermission::status() {
    if (operatingSystemAllowsNetwork() == false) {
        return Status::Blocked;
    }
    QSettings settings;
    if (settings.contains(QStringLiteral("network/allowed")) == false) {
        return Status::Undetermined;
    }
    return settings.value(QStringLiteral("network/allowed")).toBool() ? Status::Granted : Status::Denied;
}

void NetworkPermission::remember(bool allowed) {
    QSettings settings;
    settings.setValue(QStringLiteral("network/allowed"), allowed);
    settings.sync();
}

bool NetworkPermission::canOpenSystemSettings() {
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

bool NetworkPermission::openSystemSettings() {
#if defined(Q_OS_ANDROID)
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid() == false) {
        return false;
    }
    const QString package = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString(QStringLiteral("android.settings.APPLICATION_DETAILS_SETTINGS"))
                              .object<jstring>());
    QJniObject uri = QJniObject::callStaticObjectMethod(
            "android/net/Uri", "fromParts",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
            QJniObject::fromString(QStringLiteral("package")).object<jstring>(),
            QJniObject::fromString(package).object<jstring>(), nullptr);
    intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;", uri.object());
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    return true;
#else
    return false;
#endif
}

QString NetworkPermission::askText() {
    return QStringLiteral(
            "NTP Clock asks public time servers for the current time, so the display can be used to measure video "
            "latency. It sends a short time query and reads the reply. Nothing else is sent.");
}

QString NetworkPermission::blockedText() {
    return QStringLiteral(
            "Network access is turned off for NTP Clock. Open the app settings and allow network access, then return "
            "here and try again.");
}
