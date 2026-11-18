#include "NetworkPermission.h"

#include <QSettings>

#include <gtest/gtest.h>

TEST(NetworkPermission, RemembersTheChoice) {
    QSettings settings;
    settings.clear();
    settings.sync();
    EXPECT_EQ(NetworkPermission::status(), NetworkPermission::Status::Undetermined);
    NetworkPermission::remember(true);
    EXPECT_EQ(NetworkPermission::status(), NetworkPermission::Status::Granted);
    NetworkPermission::remember(false);
    EXPECT_EQ(NetworkPermission::status(), NetworkPermission::Status::Denied);
    EXPECT_FALSE(NetworkPermission::canOpenSystemSettings());
    settings.clear();
    settings.sync();
}
