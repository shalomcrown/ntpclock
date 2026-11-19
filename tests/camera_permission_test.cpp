#include "CameraPermission.h"

#include <QSettings>

#include <gtest/gtest.h>

//================================================================================
TEST(CameraPermission, RemembersTheChoice) {
    QSettings settings;
    settings.remove(QStringLiteral("camera/allowed"));
    settings.sync();
    if (CameraPermission::status() == CameraPermission::Status::Blocked) {
        GTEST_SKIP() << "the system has blocked the camera";
    }
    EXPECT_EQ(CameraPermission::status(), CameraPermission::Status::Undetermined);
    CameraPermission::remember(true);
    EXPECT_EQ(CameraPermission::status(), CameraPermission::Status::Granted);
    CameraPermission::remember(false);
    EXPECT_EQ(CameraPermission::status(), CameraPermission::Status::Denied);
    settings.remove(QStringLiteral("camera/allowed"));
    settings.sync();
}
