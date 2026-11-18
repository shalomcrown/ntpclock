#include "MainWindow.h"

#include <QPushButton>
#include <QSettings>

#include <gtest/gtest.h>

TEST(MainWindow, AsksBeforeUsingTheNetwork) {
    QSettings settings;
    settings.clear();
    settings.sync();

    MainWindow window;
    window.resize(800, 600);
    window.show();
    auto* allow = window.findChild<QPushButton*>(QStringLiteral("allowNetwork"));
    ASSERT_NE(allow, nullptr);
    EXPECT_TRUE(allow->isVisible());
}
