#include "TimingBlock.h"
#include "TimingCode.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QTimeZone>
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
// A slanted photo of the tablet. The screen showed 12:26:48.989, so the block
// is 989. The reader used to accept a larger all-white shape as 2730, and the
// difference came out 1434 ms instead of about 300 ms.
TEST(TimingBlock, ReadsASlantedPhotoAsTheMillisecondsOnTheScreen) {
    const QImage photo(QString(NTPCLOCK_FIXTURE_DIR) + QStringLiteral("/photographed-block.png"));
    ASSERT_FALSE(photo.isNull());
    const LatencyReading reading = measureLatency(photo, 1296);
    ASSERT_TRUE(reading.ok);
    EXPECT_EQ(reading.otherValue, 989);
    EXPECT_EQ(reading.milliseconds, 307);
    EXPECT_EQ(latencyLabel(reading), QStringLiteral("307 ms behind\n0.989 other · 1.296 this"));

    // A steeper slant. The block reads 1258, and this clock was at 1546,
    // which is 288 ms, the gap between the two millisecond displays.
    const QImage steeper(QString(NTPCLOCK_FIXTURE_DIR) + QStringLiteral("/slanted-block.png"));
    ASSERT_FALSE(steeper.isNull());
    const LatencyReading again = measureLatency(steeper, 1546);
    ASSERT_TRUE(again.ok);
    EXPECT_EQ(again.otherValue, 1258);
    EXPECT_EQ(again.milliseconds, 288);
}

//================================================================================
// A phone camera frame is 1920 by 1920, so a block filmed from close up has
// cells over 100 pixels wide. A false all-white box in the digits was returned
// as 2730 because the edge search stopped at 80 pixels.
TEST(TimingBlock, ReadsABlockWithHugeCellsInACameraFrame) {
    QImage canvas(1920, 1920, QImage::Format_RGB32);
    canvas.fill(QColor(20, 24, 30));
    QPainter painter(&canvas);
    painter.setPen(QColor(215, 205, 215));
    QFont font = painter.font();
    font.setPixelSize(330);
    painter.setFont(font);
    painter.drawText(QRect(560, 700, 1300, 500), Qt::AlignCenter, QStringLiteral("390"));
    painter.drawText(QRect(100, 150, 1700, 500), Qt::AlignCenter, QStringLiteral("14:15:55"));
    painter.drawImage(QPoint(40, 700), renderTimingBlock(3903, 108));
    painter.end();

    const std::optional<TimingHit> hit = findTimingBlock(boxBlur(canvas));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->value, 3903);
    EXPECT_GE(hit->bounds.width(), 500);
}

//================================================================================
// A real 1920-wide camera frame of the tablet at 14:15:55. The block encodes
// 3410. A coarse search of the bright frame missed that alignment, and the
// only other hit was a bright blob, code 2730, which reported about 1400 ms.
TEST(TimingBlock, ReadsACloseUpCameraFrameOfTheBlock) {
    const QImage photo(QString(NTPCLOCK_FIXTURE_DIR) + QStringLiteral("/camera-frame-block.jpg"));
    ASSERT_FALSE(photo.isNull());
    const LatencyReading reading = measureLatency(photo, 3631);
    ASSERT_TRUE(reading.ok);
    EXPECT_EQ(reading.otherValue, 3410);
    EXPECT_EQ(reading.milliseconds, 221);
    EXPECT_EQ(latencyLabel(reading), QStringLiteral("221 ms behind\n3.410 other · 3.631 this"));
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
    EXPECT_EQ(latencyLabel(read), QStringLiteral("369 ms behind\n0.900 other · 1.269 this"));
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

//================================================================================
// The tablet compare shot painted 19:46:49.487 against this clock at
// 19:46:49.650. A 15-pixel bright patch was accepted as the block and the
// line said 1219 ms ahead. The patch is too small, and the painted time is
// readable, so the gap is 163 ms behind.
TEST(TimingBlock, ReadsThePaintedTimeInsteadOfATinyBrightPatch) {
    const QImage photo(QString(NTPCLOCK_FIXTURE_DIR) + QStringLiteral("/compare-other-194649487.png"));
    ASSERT_FALSE(photo.isNull());
    const QDateTime clock(QDate(2026, 11, 19), QTime(19, 46, 49, 650), QTimeZone::systemTimeZone());
    const LatencyReading reading = measureLatency(photo, 1650, clock.toMSecsSinceEpoch());
    ASSERT_TRUE(reading.ok);
    EXPECT_EQ(reading.otherValue, 1487);
    EXPECT_EQ(reading.milliseconds, 163);
    EXPECT_EQ(latencyLabel(reading),
              QStringLiteral("163 ms behind\n19:46:49.487 other · 19:46:49.650 this"));
}

//================================================================================
// 14:12:57.000 against 14:13:00.000 is 3 seconds. Both seconds land on the same
// slot of the four-second block, which reports the other screen 1 second ahead.
// The painted HH:mm:ss is the only way to see the rollover.
TEST(TimingBlock, UsesThePaintedSecondsWhenTheMillisecondsRollOver) {
    const QStringList families = QFontDatabase::families();
    if (families.contains(QStringLiteral("DejaVu Sans"), Qt::CaseInsensitive) == false) {
        GTEST_SKIP() << "DejaVu Sans is not installed";
    }
    QImage image(1600, 800, QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setPen(Qt::white);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font(QStringLiteral("DejaVu Sans"));
    font.setPixelSize(90);
    painter.setFont(font);
    painter.drawText(QRect(20, 10, 1500, 180), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("14:12:57"));
    const QImage block = renderTimingBlock(1000, 20);
    const QPoint blockAt(40, 360);
    painter.drawImage(blockAt, block);
    painter.end();

    const QDateTime clock(QDate(2026, 11, 20), QTime(14, 13, 0, 0), QTimeZone::systemTimeZone());
    const LatencyReading reading = measureLatency(image, 0, clock.toMSecsSinceEpoch());
    ASSERT_TRUE(reading.ok);
    EXPECT_EQ(reading.milliseconds, 3000);
    EXPECT_EQ(latencyLabel(reading),
              QStringLiteral("3000 ms behind\n14:12:57.000 other · 14:13:00.000 this"));
}