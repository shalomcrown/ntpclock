#include "ClockWidget.h"
#include "NtpTime.h"
#include "TimingBlock.h"

#include <QColor>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>

#include <algorithm>

#include <gtest/gtest.h>

namespace {
//================================================================================
QRect ink(const QImage& image) {
    int left = image.width();
    int top = image.height();
    int right = 0;
    int bottom = 0;
    bool found = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).lightness() <= 200) {
                continue;
            }
            found = true;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    if (found == false) {
        return QRect();
    }
    return QRect(left, top, right - left + 1, bottom - top + 1);
}

//================================================================================
QImage paintDigits(const QString& text) {
    QImage image(360, 120, QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setPen(Qt::white);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(72);
    drawFixedDigits(painter, image.rect().adjusted(8, 8, -8, -8), font, text);
    return image;
}
}  // namespace

//================================================================================
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

//================================================================================
TEST(ClockWidget, ShowsTheVersionAtTheTop) {
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0.202611190840.5550b95"));
    ClockWidget widget;
    widget.resize(800, 600);
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = NtpTime::systemNowUs();
    widget.setSnapshot(snapshot);
    widget.show();

    const QImage image = widget.grab().toImage();
    int light = 0;
    for (int y = 10; y < 32; ++y) {
        for (int x = 280; x < 520; ++x) {
            if (image.pixelColor(x, y).lightness() > 140) {
                ++light;
            }
        }
    }
    EXPECT_GT(light, 20);
    EXPECT_EQ(image.pixelColor(1, 1), QColor(Qt::black));
}

//================================================================================
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

//================================================================================
TEST(ClockWidget, TimingBlockReadsBackAndClearsTheCamera) {
    ClockWidget wide;
    wide.resize(960, 540);
    wide.setInsetRect(QRect(760, 360, 180, 140));
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = NtpTime::systemNowUs();
    wide.setSnapshot(snapshot);
    wide.show();
    const QImage wideImage = wide.grab().toImage();
    const std::optional<TimingHit> wideHit = findTimingBlock(wideImage);
    ASSERT_TRUE(wideHit.has_value());
    EXPECT_EQ(wideHit->value, wide.timingValue());
    EXPECT_FALSE(wideHit->bounds.intersects(QRect(760, 360, 180, 140)));

    ClockWidget tall;
    tall.resize(420, 900);
    tall.setSnapshot(snapshot);
    tall.show();
    const QImage tallImage = tall.grab().toImage();
    const std::optional<TimingHit> tallHit = findTimingBlock(tallImage);
    ASSERT_TRUE(tallHit.has_value());
    EXPECT_EQ(tallHit->value, tall.timingValue());
    EXPECT_GT(tallHit->bounds.top(), tallImage.height() / 3);
}

//================================================================================
TEST(ClockDigits, NarrowAndWideDigitsStayInTheSameCells) {
    const QImage narrow = paintDigits(QStringLiteral("111"));
    const QImage wide = paintDigits(QStringLiteral("888"));
    const QRect narrowInk = ink(narrow);
    const QRect wideInk = ink(wide);
    ASSERT_FALSE(narrowInk.isEmpty());
    ASSERT_FALSE(wideInk.isEmpty());
    // "1" is narrower than "8", so the ink sits inside the cell. The line
    // itself stays put: a centered proportional string would shift by much more.
    EXPECT_LT(std::abs(narrowInk.left() - wideInk.left()), 16);
    EXPECT_LT(std::abs(narrowInk.right() - wideInk.right()), 16);
}
