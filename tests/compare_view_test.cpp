#include "CompareView.h"

#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPushButton>

#include <gtest/gtest.h>

//================================================================================
TEST(CompareView, ShowsThePhotoBesideTheClock) {
    CompareView view;
    view.resize(600, 400);
    QImage other(80, 80, QImage::Format_RGB32);
    other.fill(Qt::red);
    QImage clock(80, 80, QImage::Format_RGB32);
    clock.fill(Qt::blue);
    view.setPair(other, clock);
    view.show();

    const QImage image = view.grab().toImage();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.pixelColor(120, 180), QColor(Qt::red));
    EXPECT_EQ(image.pixelColor(460, 180), QColor(Qt::blue));

    auto* back = view.findChild<QPushButton*>(QStringLiteral("compareBack"));
    ASSERT_NE(back, nullptr);
    EXPECT_TRUE(back->isVisible());
}

//================================================================================
TEST(CompareView, ShowsTheLatency) {
    CompareView view;
    view.resize(600, 400);
    view.setPair(QImage(80, 80, QImage::Format_RGB32), QImage(80, 80, QImage::Format_RGB32),
                 QStringLiteral("369 ms behind\n.900 other · .269 this"));
    view.show();

    auto* result = view.findChild<QLabel*>(QStringLiteral("compareLatency"));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->isVisible());
    EXPECT_EQ(result->text(), QStringLiteral("369 ms behind\n.900 other · .269 this"));
}
