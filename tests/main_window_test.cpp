#include "MainWindow.h"

#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTextBrowser>
#include <QWidget>

#include <gtest/gtest.h>

//================================================================================
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

//================================================================================
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

//================================================================================
TEST(MainWindow, SelfieButtonDoesNotTakeAPicture) {
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
    auto* facing = window.findChild<QWidget*>(QStringLiteral("cameraFacing"));
    ASSERT_NE(inset, nullptr);
    ASSERT_NE(facing, nullptr);
    EXPECT_TRUE(facing->isVisible());
    EXPECT_LE(facing->width(), 36);
    EXPECT_LE(facing->height(), 36);
    EXPECT_LT(facing->width(), inset->width() / 3);
    EXPECT_GT(facing->x(), inset->width() / 2);

    const QPointF local(facing->width() / 2.0, facing->height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, local, facing->mapToGlobal(local.toPoint()), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(facing, &press);

    auto* allowCamera = window.findChild<QPushButton*>(QStringLiteral("allowCamera"));
    ASSERT_NE(allowCamera, nullptr);
    EXPECT_FALSE(allowCamera->isVisible());
}

//================================================================================
TEST(MainWindow, GuideShowsTheVersionAndTheReadme) {
    QSettings settings;
    settings.clear();
    settings.sync();

    MainWindow window;
    window.resize(800, 600);
    window.show();
    auto* allow = window.findChild<QPushButton*>(QStringLiteral("allowNetwork"));
    ASSERT_NE(allow, nullptr);
    allow->click();

    auto* guideButton = window.findChild<QPushButton*>(QStringLiteral("guideButton"));
    ASSERT_NE(guideButton, nullptr);
    EXPECT_TRUE(guideButton->isVisible());
    guideButton->click();

    auto* version = window.findChild<QLabel*>(QStringLiteral("guideVersion"));
    auto* text = window.findChild<QTextBrowser*>(QStringLiteral("guideText"));
    ASSERT_NE(version, nullptr);
    ASSERT_NE(text, nullptr);
    EXPECT_TRUE(version->isVisible());
    EXPECT_EQ(version->text(), QCoreApplication::applicationVersion());
    EXPECT_TRUE(text->toPlainText().contains(QStringLiteral("Photographing another screen")));
    EXPECT_FALSE(guideButton->isVisible());

    auto* back = window.findChild<QPushButton*>(QStringLiteral("guideBack"));
    ASSERT_NE(back, nullptr);
    back->click();
    EXPECT_TRUE(guideButton->isVisible());
}
