#pragma once

#include <QMargins>
#include <QSize>

//================================================================================
// The part of a full-screen window that is really on the display. Qt 6.6 on
// Android can size the window to the whole screen while Android keeps it below
// the camera cutout, which pushes the bottom (portrait) or the right side
// (landscape) off the display.
class VisibleArea {
public:
//================================================================================
    // Size in device pixels of the Android activity content view, or an invalid
    // size when the platform does not report one.
    static QSize devicePixels();
//================================================================================
    // Right and bottom margins that keep a widget of widgetSize inside the
    // visible device pixels.
    static QMargins hiddenMargins(const QSize& widgetSize, const QSize& visiblePixels, qreal devicePixelRatio);
};
