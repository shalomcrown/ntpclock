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

//================================================================================
// The compare page is filled in before it is shown. The latency band has to
// keep its height then, because showing the page does not resize it.
TEST(CompareView, ShowsTheLatencyBeforeThePageIsShown) {
    CompareView view;
    view.resize(443, 900);
    view.setPair(QImage(80, 80, QImage::Format_RGB32), QImage(80, 80, QImage::Format_RGB32),
                 QStringLiteral("369 ms behind\n.900 other · .269 this"));

    auto* result = view.findChild<QLabel*>(QStringLiteral("compareLatency"));
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(view.isVisible());
    EXPECT_EQ(result->y(), 8);
    EXPECT_EQ(result->height(), 88);

    view.show();
    const QImage image = view.grab().toImage();
    ASSERT_FALSE(image.isNull());
    int bright = 0;
    const QRect band = result->geometry();
    for (int y = band.top(); y <= band.bottom(); y += 2) {
        for (int x = band.left(); x <= band.right(); x += 4) {
            if (image.pixelColor(x, y).lightness() > 180) {
                ++bright;
            }
        }
    }
    EXPECT_GT(bright, 10);
}
