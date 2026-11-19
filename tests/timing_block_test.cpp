#include "TimingBlock.h"
#include "TimingCode.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QTransform>

#include <algorithm>

#include <gtest/gtest.h>

//================================================================================
TEST(TimingCode, WrapsTheTimestampIntoFourSeconds) {
    EXPECT_EQ(timingValueFromUs(0), 0);
    EXPECT_EQ(timingValueFromUs(932 * 1000), 932);
    EXPECT_EQ(timingValueFromUs(5 * 1000000LL + 10 * 1000), 1010);
    EXPECT_EQ(timingValueFromUs(-10 * 1000), kTimingModulus - 10);
}

//================================================================================
TEST(TimingCode, LatencyTakesTheShortWayAroundTheWindow) {
    EXPECT_EQ(latencyMs(1301, 932), 369);
    EXPECT_EQ(latencyMs(10, 3990), 20);
    EXPECT_EQ(latencyMs(100, 250), -150);
    EXPECT_EQ(latencyMs(400, 400), 0);
}

//================================================================================
TEST(TimingBlock, RoundTripsEveryValue) {
    for (int value = 0; value < kTimingModulus; ++value) {
        const std::optional<TimingHit> hit = findTimingBlock(renderTimingBlock(value, 4));
        ASSERT_TRUE(hit.has_value()) << value;
        ASSERT_EQ(hit->value, value) << value;
    }
}

//================================================================================
TEST(TimingBlock, DataSquaresMatchTheFrame) {
    const int cell = 48;
    const QImage image = renderTimingBlock(1, cell);
    const int mid = cell / 2;
    auto whiteRun = [&](int x, int y, bool horizontal) {
        auto lit = [&](int px, int py) {
            if (px < 0 || py < 0 || px >= image.width() || py >= image.height()) {
                return false;
            }
            return image.pixelColor(px, py).lightness() > 200;
        };
        int length = 0;
        if (horizontal) {
            int left = x;
            while (lit(left - 1, y)) {
                --left;
            }
            int right = x;
            while (lit(right + 1, y)) {
                ++right;
            }
            length = right - left + 1;
        } else {
            int top = y;
            while (lit(x, top - 1)) {
                --top;
            }
            int bottom = y;
            while (lit(x, bottom + 1)) {
                ++bottom;
            }
            length = bottom - top + 1;
        }
        return length;
    };
    const int frame = whiteRun(cell + mid, mid, false);
    const int data = whiteRun(3 * cell + mid, 4 * cell + mid, true);
    EXPECT_EQ(data, frame);
    EXPECT_GT(frame, cell / 2);
    EXPECT_LT(frame, cell);
}

//================================================================================
QImage boxBlur(const QImage& source) {
    const QImage gray = source.convertToFormat(QImage::Format_Grayscale8);
    QImage blurred(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int px = std::clamp(x + dx, 0, gray.width() - 1);
                    const int py = std::clamp(y + dy, 0, gray.height() - 1);
                    sum += qGray(gray.pixel(px, py));
                    ++count;
                }
            }
            blurred.setPixel(x, y, qRgb(sum / count, sum / count, sum / count));
        }
    }
    return blurred;
}

//================================================================================
TEST(TimingBlock, ReadsASoftPhotoAndRejectsAOnePixelShift) {
    const QImage sharp = renderTimingBlock(1500, 14);
    const std::optional<TimingHit> soft = findTimingBlock(boxBlur(sharp));
    ASSERT_TRUE(soft.has_value());
    EXPECT_EQ(soft->value, 1500);

    QImage shifted(sharp.size(), sharp.format());
    shifted.fill(Qt::black);
    QPainter painter(&shifted);
    painter.drawImage(-1, 0, sharp);
    painter.end();
    const std::optional<TimingHit> hit = findTimingBlock(shifted);
    if (hit.has_value()) {
        EXPECT_EQ(hit->value, 1500);
    }
}

//================================================================================
TEST(TimingBlock, FindsTheBlockAmongOtherWhiteShapes) {
    const QImage block = renderTimingBlock(2468, 10);
    QImage canvas(360, 280, QImage::Format_RGB32);
    canvas.fill(Qt::black);
    QPainter painter(&canvas);
    painter.fillRect(QRect(12, 16, 36, 90), Qt::white);
    painter.fillRect(QRect(230, 16, 70, 70), Qt::white);
    painter.drawImage(QPoint(70, 120), block);

    const std::optional<TimingHit> hit = findTimingBlock(canvas);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->value, 2468);
    EXPECT_TRUE(hit->bounds.contains(QPoint(70 + 8, 120 + 8)));
}

//================================================================================
TEST(TimingBlock, ReadsAMirrorAndAQuarterTurn) {
    const QImage block = renderTimingBlock(3210, 8);
    const std::optional<TimingHit> mirrored = findTimingBlock(block.mirrored(true, false));
    ASSERT_TRUE(mirrored.has_value());
    EXPECT_EQ(mirrored->value, 3210);

    const QImage turned = block.transformed(QTransform().rotate(90));
    const std::optional<TimingHit> rotated = findTimingBlock(turned);
    ASSERT_TRUE(rotated.has_value());
    EXPECT_EQ(rotated->value, 3210);
}

//================================================================================
TEST(TimingBlock, SaysWhenThePictureHasNoBlock) {
    QImage blank(80, 80, QImage::Format_RGB32);
    blank.fill(Qt::black);
    const LatencyReading missing = measureLatency(blank, 100);
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(latencyLabel(missing), QStringLiteral("Could not read the timing block in the other picture."));

    const LatencyReading read = measureLatency(renderTimingBlock(900, 6), 1269);
    ASSERT_TRUE(read.ok);
    EXPECT_EQ(read.milliseconds, 369);
    EXPECT_EQ(latencyLabel(read), QStringLiteral("369 ms behind\n.900 other · .269 this"));
}

//================================================================================
TEST(TimingPlacement, SitsBelowTheDigitsWhenTheBandIsTall) {
    const QRect band(10, 20, 400, 520);
    const TimingPlacement placed = placeTimingBlock(band, 390, 400, QRect());
    EXPECT_FALSE(placed.block.isEmpty());
    EXPECT_GE(placed.digits.width(), 390);
    EXPECT_GE(placed.digits.height(), 400);
    EXPECT_GT(placed.block.top(), placed.digits.top());
    EXPECT_FALSE(placed.block.intersects(placed.digits));
    EXPECT_TRUE(band.contains(placed.block));
}

//================================================================================
TEST(TimingPlacement, SitsBesideTheDigitsWhenTheBandIsWide) {
    const QRect band(0, 0, 800, 180);
    const QRect avoid(620, 40, 180, 140);
    const TimingPlacement placed = placeTimingBlock(band, 280, 160, avoid);
    EXPECT_FALSE(placed.block.isEmpty());
    EXPECT_GE(placed.digits.width(), 280);
    EXPECT_GE(placed.digits.height(), 160);
    EXPECT_FALSE(placed.block.intersects(placed.digits));
    EXPECT_FALSE(placed.block.intersects(avoid));
    EXPECT_LT(placed.block.right(), avoid.left());
}

//================================================================================
TEST(TimingBlock, SearchStaysQuickOnAClockSizedImage) {
    QImage canvas(960, 540, QImage::Format_RGB32);
    canvas.fill(Qt::black);
    QPainter painter(&canvas);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(120);
    painter.setFont(font);
    painter.drawText(canvas.rect(), Qt::AlignCenter, QStringLiteral("08:47:05"));
    painter.drawImage(QPoint(700, 360), renderTimingBlock(1500, 14));

    QElapsedTimer timer;
    timer.start();
    const std::optional<TimingHit> hit = findTimingBlock(canvas);
    const qint64 elapsed = timer.elapsed();
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->value, 1500);
    EXPECT_LT(elapsed, 1500);
}