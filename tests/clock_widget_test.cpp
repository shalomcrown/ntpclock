#include "ClockWidget.h"
#include "NtpTime.h"

#include <QColor>
#include <QImage>

#include <gtest/gtest.h>

TEST(ClockWidget, PaintsLightDigitsOnBlack) {
    ClockWidget widget;
    widget.resize(800, 600);
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = NtpTime::systemNowUs();
    snapshot.server = QStringLiteral("time.cloudflare.com");
    widget.setSnapshot(snapshot);
    widget.show();
    const QImage image = widget.grab().toImage();
    ASSERT_FALSE(image.isNull());
    ASSERT_EQ(image.width(), 800);
    EXPECT_EQ(image.pixelColor(1, 1), QColor(Qt::black));

    bool sawLight = false;
    for (int y = 0; y < image.height() && sawLight == false; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).lightness() > 200) {
                sawLight = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sawLight);
}

TEST(ClockWidget, KeepsDigitsClearOfTheCameraCorner) {
    ClockWidget widget;
    widget.resize(1280, 720);
    widget.setInsetRect(QRect(944, 464, 320, 240));
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = NtpTime::systemNowUs();
    snapshot.server = QStringLiteral("time.cloudflare.com");
    widget.setSnapshot(snapshot);
    widget.show();

    const QImage image = widget.grab().toImage();
    ASSERT_EQ(image.size(), QSize(1280, 720));
    EXPECT_EQ(image.pixelColor(1100, 560), QColor(Qt::black));

    bool sawLight = false;
    for (int y = 400; y < 640 && sawLight == false; ++y) {
        for (int x = 80; x < 900; ++x) {
            if (image.pixelColor(x, y).lightness() > 200) {
                sawLight = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sawLight);
}
