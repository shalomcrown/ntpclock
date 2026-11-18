#pragma once

#include "ClockFormat.h"

#include <QWidget>

#include <functional>

class ClockWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClockWidget(QWidget* parent = nullptr);

    void setSource(std::function<ClockSnapshot()> source);
    void setSnapshot(const ClockSnapshot& snapshot);
    // Rectangle, in this widget's coordinates, covered by the camera inset.
    // Bands that meet it are narrowed so the digits stay fully visible.
    void setInsetRect(const QRect& rect);

signals:
    void enableRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::function<ClockSnapshot()> m_source;
    ClockSnapshot m_snapshot;
    QRect m_insetRect;
};
