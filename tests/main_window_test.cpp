#include "MainWindow.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QWidget>

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

TEST(MainWindow, TappingTheInsetAsksForTheCamera) {
    QSettings settings;
    settings.clear();
    settings.sync();

    MainWindow window;
    window.resize(800, 600);
    window.show();

    auto* allowNetwork = window.findChild<QPushButton*>(QStringLiteral("allowNetwork"));
    ASSERT_NE(allowNetwork, nullptr);
    allowNetwork->click();

    auto* inset = window.findChild<QWidget*>(QStringLiteral("cameraInset"));
    ASSERT_NE(inset, nullptr);
    EXPECT_TRUE(inset->isVisible());
    EXPECT_GT(inset->x(), window.width() / 2);
    EXPECT_GT(inset->y(), window.height() / 2);

    const QPointF local(inset->width() / 2.0, inset->height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, local, inset->mapToGlobal(local.toPoint()), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(inset, &press);

    auto* allowCamera = window.findChild<QPushButton*>(QStringLiteral("allowCamera"));
    ASSERT_NE(allowCamera, nullptr);
    EXPECT_TRUE(allowCamera->isVisible());
}
