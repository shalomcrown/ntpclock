#include "ClockFormat.h"

#include <QDateTime>
#include <QTimeZone>

#include <gtest/gtest.h>

namespace {
//================================================================================
qint64 at(int hour, int minute, int second, int millisecond) {
    const QDateTime when(QDate(2026, 11, 18), QTime(hour, minute, second, millisecond), QTimeZone::utc());
    return when.toMSecsSinceEpoch() * 1000;
}
}  // namespace

//================================================================================
TEST(ClockFormat, SplitsTimeAndMilliseconds) {
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = at(12, 34, 56, 427);
    snapshot.server = QStringLiteral("time.cloudflare.com");
    const ClockLines lines = formatClock(snapshot.lastNtpUs, snapshot, QTimeZone::utc());
    EXPECT_EQ(lines.date, QStringLiteral("2026-11-18"));
    EXPECT_EQ(lines.time, QStringLiteral("12:34:56"));
    EXPECT_EQ(lines.millis, QStringLiteral("427"));
    EXPECT_TRUE(lines.footer.contains(QStringLiteral("12:34:56.427")));
    EXPECT_TRUE(lines.footer.contains(QStringLiteral("time.cloudflare.com")));
    EXPECT_TRUE(lines.footer.contains(QStringLiteral("0 s ago")));
    EXPECT_FALSE(lines.warning);
}

//================================================================================
TEST(ClockFormat, RoundsUpIntoTheNextSecond) {
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    const qint64 displayUs = at(1, 2, 3, 999) + 600;
    snapshot.lastNtpUs = displayUs;
    const ClockLines lines = formatClock(displayUs, snapshot, QTimeZone::utc());
    EXPECT_EQ(lines.time, QStringLiteral("01:02:04"));
    EXPECT_EQ(lines.millis, QStringLiteral("000"));
}

//================================================================================
TEST(ClockFormat, WaitingAndDeniedFooters) {
    const qint64 now = at(8, 0, 0, 0);
    ClockSnapshot waiting;
    waiting.networkAllowed = true;
    const ClockLines waitingLines = formatClock(now, waiting, QTimeZone::utc());
    EXPECT_EQ(waitingLines.footer, QStringLiteral("Waiting for NTP"));
    EXPECT_TRUE(waitingLines.warning);

    ClockSnapshot denied;
    const ClockLines deniedLines = formatClock(now, denied, QTimeZone::utc());
    EXPECT_EQ(deniedLines.footer, QStringLiteral("Network access not allowed. Tap to allow."));

    ClockSnapshot failed = waiting;
    failed.error = QStringLiteral("No reply from time servers");
    EXPECT_EQ(formatClock(now, failed, QTimeZone::utc()).footer, QStringLiteral("No reply from time servers"));
}

//================================================================================
TEST(ClockFormat, ShowsAgeSinceTheLastSync) {
    ClockSnapshot snapshot;
    snapshot.networkAllowed = true;
    snapshot.synced = true;
    snapshot.lastNtpUs = at(12, 0, 0, 0);
    snapshot.server = QStringLiteral("pool.ntp.org");
    const ClockLines lines = formatClock(snapshot.lastNtpUs + 125LL * 1000000LL, snapshot, QTimeZone::utc());
    EXPECT_TRUE(lines.footer.contains(QStringLiteral("2 min ago")));
    EXPECT_TRUE(lines.footer.contains(QStringLiteral("pool.ntp.org")));
}
