#include "DigitRead.h"
#include "TimingBlock.h"

#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>

#include <algorithm>

#include <gtest/gtest.h>

namespace {
//================================================================================
bool hasFamily(const QString& name) {
    const QStringList families = QFontDatabase::families();
    for (const QString& family : families) {
        if (family.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

//================================================================================
QImage tightDigit(const QString& family, QChar digit) {
    QImage image(240, 280, QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setPen(Qt::white);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font(family);
    font.setPixelSize(160);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, QString(digit));
    painter.end();

    int left = image.width();
    int right = 0;
    int top = image.height();
    int bottom = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qGray(image.pixel(x, y)) > 40) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    return image.copy(QRect(left, top, right - left + 1, bottom - top + 1));
}
}  // namespace

//================================================================================
// The other screen may not use this device's font. The reader has to accept
// a sans, a mono, and a serif by shape, and refuse to guess when it cannot.
TEST(DigitRead, ReadsRegularFontsByShape) {
    const QStringList families = {QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Sans Mono"),
                                  QStringLiteral("Liberation Sans"), QStringLiteral("Liberation Serif"),
                                  QStringLiteral("FreeSans"), QStringLiteral("Carlito")};
    int checked = 0;
    for (const QString& family : families) {
        if (hasFamily(family) == false) {
            continue;
        }
        ++checked;
        for (int digit = 0; digit <= 9; ++digit) {
            const QChar glyph(QLatin1Char('0' + digit));
            EXPECT_EQ(readDigit(tightDigit(family, glyph)), digit) << family.toStdString() << " " << digit;
        }
    }
    EXPECT_GT(checked, 0);
}

//================================================================================
PaintedClock paintClock(const QString& family, int pixelSize) {
    QImage image(1600, 800, QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setPen(Qt::white);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font(family);
    font.setPixelSize(pixelSize);
    painter.setFont(font);
    painter.drawText(QRect(20, 10, 1500, pixelSize * 2), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("19:46:49"));
    // The block stays the same size. The digits do not.
    const QRect block(40, 320, 240, 280);
    painter.drawText(QRect(block.right() + pixelSize / 5, block.top(), 900, block.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("487"));
    painter.end();
    return readPaintedClock(image, block);
}

//================================================================================
// A small drawing and a large one of the same time. Nothing in the reader may
// depend on the digits being a particular number of pixels.
TEST(DigitRead, ReadsATimeLineAndTheMillisecondsBesideABlock) {
    if (hasFamily(QStringLiteral("DejaVu Sans")) == false) {
        GTEST_SKIP() << "DejaVu Sans is not installed";
    }
    for (const int pixelSize : {36, 120}) {
        const PaintedClock painted = paintClock(QStringLiteral("DejaVu Sans"), pixelSize);
        EXPECT_TRUE(painted.timeOk) << pixelSize;
        EXPECT_EQ(painted.hour, 19) << pixelSize;
        EXPECT_EQ(painted.minute, 46) << pixelSize;
        EXPECT_EQ(painted.second, 49) << pixelSize;
        EXPECT_TRUE(painted.millisOk) << pixelSize;
        EXPECT_EQ(painted.millis, 487) << pixelSize;
    }
}

//================================================================================
TEST(DigitRead, LeavesABareTimingBlockUnread) {
    const QImage block = renderTimingBlock(1487, 16);
    const PaintedClock painted = readPaintedClock(block, block.rect());
    EXPECT_FALSE(painted.timeOk);
    EXPECT_FALSE(painted.millisOk);
}
