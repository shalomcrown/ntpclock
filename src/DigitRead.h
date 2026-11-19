#pragma once

#include <QImage>
#include <QRect>

//================================================================================
// A clock painted on the other screen. Digits are accepted only when the
// shape is unambiguous. A blur, a hole that does not match, or a line that
// is not HH:mm:ss leaves the matching field unread.
struct PaintedClock {
    bool timeOk = false;
    int hour = -1;
    int minute = -1;
    int second = -1;
    bool millisOk = false;
    int millis = -1;
};

//================================================================================
// 0-9 from the ink shape (holes and where the stroke sits). -1 when the
// crop does not look like one digit. This does not compare the glyph to a font.
int readDigit(const QImage& glyph);

//================================================================================
// Look above `block` for HH:mm:ss and beside it for three millisecond digits.
// Colons are the short marks between the pairs. Other text is ignored.
PaintedClock readPaintedClock(const QImage& image, const QRect& block);
