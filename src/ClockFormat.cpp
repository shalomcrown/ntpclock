#include "ClockFormat.h"

#include <QDateTime>
#include <QLocale>

namespace {
qint64 roundUsToMs(qint64 microseconds) {
    if (microseconds >= 0) {
        return (microseconds + 500) / 1000;
    }
    return -((-microseconds + 500) / 1000);
}

QString agePhrase(qint64 ageUs) {
    if (ageUs < 0) {
        ageUs = 0;
    }
    const qint64 seconds = ageUs / 1000000;
    if (seconds < 60) {
        return QStringLiteral("%1 s ago").arg(seconds);
    }
    if (seconds < 3600) {
        return QStringLiteral("%1 min ago").arg(seconds / 60);
    }
    return QStringLiteral("%1 h ago").arg(seconds / 3600);
}
}  // namespace

ClockLines formatClock(qint64 displayUs, const ClockSnapshot& snapshot, const QTimeZone& zone) {
    const QLocale latin = QLocale::c();
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(roundUsToMs(displayUs), zone);
    ClockLines lines;
    lines.date = latin.toString(when, QStringLiteral("yyyy-MM-dd"));
    lines.time = latin.toString(when, QStringLiteral("HH:mm:ss"));
    lines.millis = latin.toString(when, QStringLiteral("zzz"));

    if (snapshot.networkAllowed == false) {
        lines.footer = QStringLiteral("Network access not allowed. Tap to allow.");
        lines.warning = true;
        return lines;
    }
    if (snapshot.synced == false) {
        lines.footer = snapshot.error.isEmpty() ? QStringLiteral("Waiting for NTP") : snapshot.error;
        lines.warning = true;
        return lines;
    }

    const QDateTime last = QDateTime::fromMSecsSinceEpoch(roundUsToMs(snapshot.lastNtpUs), zone);
    lines.footer = QStringLiteral("Last NTP %1").arg(latin.toString(last, QStringLiteral("HH:mm:ss.zzz")));
    if (snapshot.server.isEmpty() == false) {
        lines.footer += QStringLiteral(" · ") + snapshot.server;
    }
    lines.footer += QStringLiteral(" · ") + agePhrase(displayUs - snapshot.lastNtpUs);
    if (snapshot.error.isEmpty() == false) {
        lines.footer += QStringLiteral(" · ") + snapshot.error;
    }
    lines.warning = snapshot.error.isEmpty() == false;
    return lines;
}
