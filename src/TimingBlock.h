#pragma once

#include "TimingCode.h"

#include <QImage>
#include <QRect>
#include <QString>

#include <optional>

class QPainter;

//================================================================================
// The painted grid is the 3 by 4 data cells plus a one-cell frame. The
// top-left frame cell is missing so a turned or mirrored photo has one
// right way up.
inline constexpr int kTimingGridCols = 5;
inline constexpr int kTimingGridRows = 6;

//================================================================================
struct TimingPlacement {
    QRect digits;
    QRect block;
};

//================================================================================
struct TimingHit {
    int value = -1;
    QRect bounds;
};

//================================================================================
struct LatencyReading {
    bool ok = false;
    int milliseconds = 0;
    int clockValue = -1;
    int otherValue = -1;
    // Set when the painted HH:mm:ss was read, with milliseconds from the digits
    // or from the block. Empty when the gap comes from the block alone.
    QString otherClock;
    QString thisClock;
};

//================================================================================
// Digits keep the size they already have. The block uses whichever slack is
// larger: beside them when the band is wide, below them when it is tall.
// `avoid` is the camera inset; the block goes on the other side.
TimingPlacement placeTimingBlock(const QRect& band, int digitWidth, int digitHeight, const QRect& avoid);

//================================================================================
void paintTimingBlock(QPainter& painter, const QRect& bounds, int value);

//================================================================================
QImage renderTimingBlock(int value, int cellPx);

//================================================================================
std::optional<TimingHit> findTimingBlock(const QImage& image);

//================================================================================
// `clockDisplayMs` is the milliseconds since the epoch painted on this clock,
// or -1 when the caller only has the four-second timing code.
LatencyReading measureLatency(const QImage& otherScreen, int clockValue, qint64 clockDisplayMs = -1);

//================================================================================
QString latencyLabel(const LatencyReading& reading);
