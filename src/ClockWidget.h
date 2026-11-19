#pragma once

#include "ClockFormat.h"

#include <QFont>
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

signals:
//================================================================================
    void enableRequested();

protected:
//================================================================================
    void paintEvent(QPaintEvent* event) override;
//================================================================================
    void mousePressEvent(QMouseEvent* event) override;

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
    QRect m_footerBand;
    QFont m_versionFont;
    QFont m_dateFont;
    QFont m_timeFont;
    QFont m_millisFont;
    QFont m_footerFont;
};
