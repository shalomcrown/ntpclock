#include "CameraPermission.h"

#include "NetworkPermission.h"

#include <QCoreApplication>
#include <QSettings>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif

namespace {
enum class OsStatus { Granted, Denied, Undetermined };

//================================================================================
OsStatus operatingSystemStatus() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    switch (qApp->checkPermission(QCameraPermission())) {
    case Qt::PermissionStatus::Granted:
        return OsStatus::Granted;
    case Qt::PermissionStatus::Denied:
        return OsStatus::Denied;
    case Qt::PermissionStatus::Undetermined:
        return OsStatus::Undetermined;
    }
    return OsStatus::Undetermined;
#else
    return OsStatus::Granted;
#endif
}
}  // namespace

//================================================================================
CameraPermission::Status CameraPermission::status() {
    const OsStatus os = operatingSystemStatus();
    if (os == OsStatus::Denied) {
        return Status::Blocked;
    }
    if (os == OsStatus::Undetermined) {
        return Status::Undetermined;
    }
    QSettings settings;
    if (settings.contains(QStringLiteral("camera/allowed")) == false) {
        return Status::Undetermined;
    }
    return settings.value(QStringLiteral("camera/allowed")).toBool() ? Status::Granted : Status::Denied;
}

//================================================================================
void CameraPermission::request(const std::function<void(Status)>& done) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (operatingSystemStatus() == OsStatus::Undetermined) {
        qApp->requestPermission(QCameraPermission(), qApp, [done](const QPermission& permission) {
            if (permission.status() == Qt::PermissionStatus::Granted) {
                remember(true);
                done(Status::Granted);
            } else if (permission.status() == Qt::PermissionStatus::Denied) {
                done(Status::Blocked);
            } else {
                done(Status::Undetermined);
            }
        });
        return;
    }
#endif
    remember(true);
    done(status());
}

//================================================================================
void CameraPermission::remember(bool allowed) {
    QSettings settings;
    settings.setValue(QStringLiteral("camera/allowed"), allowed);
    settings.sync();
}

//================================================================================
bool CameraPermission::canOpenSystemSettings() {
    return NetworkPermission::canOpenSystemSettings();
}

//================================================================================
bool CameraPermission::openSystemSettings() {
    return NetworkPermission::openSystemSettings();
}

//================================================================================
QString CameraPermission::askText() {
    return QStringLiteral(
            "NTP Clock uses the camera to photograph the other screen. The picture stays on this device and is shown "
            "next to this clock, so the two times can be compared.");
}

//================================================================================
QString CameraPermission::blockedText() {
    return QStringLiteral(
            "Camera access is turned off for NTP Clock. Open the app settings and allow the camera, then return here "
            "and try again.");
}
