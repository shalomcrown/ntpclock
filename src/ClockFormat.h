#pragma once

#include <QString>
#include <QTimeZone>
#include <QtGlobal>

//================================================================================
// What the clock is currently trusting. The widget reads a copy of this.
struct ClockSnapshot {
    bool networkAllowed = false;
    bool synced = false;
    qint64 offsetUs = 0;
    qint64 lastNtpUs = 0;
    QString server;
    QString error;
};

//================================================================================
// The four lines painted on screen. Digits are ASCII, independent of locale.
struct ClockLines {
    QString date;
    QString time;
    QString millis;
    QString footer;
    bool warning = true;
};

//================================================================================
ClockLines formatClock(qint64 displayUs, const ClockSnapshot& snapshot, const QTimeZone& zone);
