#pragma once

#include "ClockFormat.h"

#include <QFont>
#include <QImage>
#include <QRect>
#include <QWidget>

#include <functional>

class QPainter;

//================================================================================
// Draws each character in a cell the width of "0", so a proportional font
// cannot shift the line as the digits change.
void drawFixedDigits(QPainter& painter, const QRect& band, const QFont& font, const QString& text);

//================================================================================
class ClockWidget : public QWidget {
    Q_OBJECT

public:
//================================================================================
    explicit ClockWidget(QWidget* parent = nullptr);

//================================================================================
    void setSource(std::function<ClockSnapshot()> source);
//================================================================================
    void setSnapshot(const ClockSnapshot& snapshot);
//================================================================================
    // Rectangle, in this widget's coordinates, covered by the camera inset.
    // Bands that meet it are narrowed so the digits stay fully visible.
    void setInsetRect(const QRect& rect);
//================================================================================
    // Value painted into the timing block on the last frame, or -1.
    int timingValue() const;
//================================================================================
    // Milliseconds since the epoch painted on the last frame, or -1.
    qint64 displayEpochMs() const;

signals:
//================================================================================
    void enableRequested();

protected:
//================================================================================
    void paintEvent(QPaintEvent* event) override;
//================================================================================
    void mousePressEvent(QMouseEvent* event) override;
//================================================================================
    void drawMillis(QPainter& painter, const QString& text);

private:
    std::function<ClockSnapshot()> m_source;
    ClockSnapshot m_snapshot;
    QRect m_insetRect;

    // Layout depends on the widget size and the inset, not on the digits.
    // Rebuilding it on every 8 ms tick probes huge fonts in portrait and
    // makes the clock stutter.
    bool m_layoutReady = false;
    QSize m_layoutSize;
    QRect m_layoutInset;
    QString m_layoutFooter;
    QRect m_versionBand;
    QRect m_dateBand;
    QRect m_timeBand;
    QRect m_millisBand;
    QRect m_millisDigitBand;
    QRect m_timingBlock;
    QRect m_footerBand;
    int m_timingValue = -1;
    qint64 m_displayEpochMs = -1;
    qint64 m_shownSecond = -1;
    int m_digitCell = 0;
    QImage m_digitSprite[10];
    QFont m_spriteFont;
    int m_spriteCell = 0;
    int m_spriteHeight = 0;
    QFont m_versionFont;
    QFont m_dateFont;
    QFont m_timeFont;
    QFont m_millisFont;
    QFont m_footerFont;
};
