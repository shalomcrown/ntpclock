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
