#include "NtpTime.h"

#include <gtest/gtest.h>

//================================================================================
TEST(NtpTime, RequestPacketIsAClientQuery) {
    const QByteArray packet = NtpTime::requestPacket(1500000);
    ASSERT_EQ(packet.size(), NtpTime::kPacketBytes);
    EXPECT_EQ(static_cast<unsigned char>(packet[0]), 0x23);
}

//================================================================================
TEST(NtpTime, ExchangeOffsetAndDelay) {
    const NtpMeasurement measurement = NtpTime::fromExchange(10000000, 10100000, 10120000, 10150000);
    EXPECT_TRUE(measurement.ok);
    EXPECT_EQ(measurement.offsetUs, 35000);
    EXPECT_EQ(measurement.delayUs, 130000);
}

//================================================================================
TEST(NtpTime, RejectsANegativeDelay) {
    const NtpMeasurement measurement = NtpTime::fromExchange(1000, 0, 0, 0);
    EXPECT_FALSE(measurement.ok);
}

//================================================================================
TEST(NtpTime, DecodesAMatchingReply) {
    const qint64 t1 = 1500000;
    const QByteArray request = NtpTime::requestPacket(t1);
    QByteArray reply(NtpTime::kPacketBytes, 0);
    reply[0] = 0x24;
    reply[1] = 1;
    for (int i = 0; i < 8; ++i) {
        reply[24 + i] = request[40 + i];
        reply[32 + i] = request[40 + i];
        reply[40 + i] = request[40 + i];
    }
    const NtpMeasurement measurement = NtpTime::decode(reply, t1, t1);
    EXPECT_TRUE(measurement.ok);
    EXPECT_EQ(measurement.offsetUs, 0);
    EXPECT_EQ(measurement.delayUs, 0);

    reply[24] = static_cast<char>(reply[24] ^ 0xff);
    EXPECT_FALSE(NtpTime::decode(reply, t1, t1).ok);
}

//================================================================================
TEST(NtpTime, RejectsAnUnsynchronizedServer) {
    QByteArray reply(NtpTime::kPacketBytes, 0);
    reply[0] = static_cast<char>(0xE4);
    reply[1] = 1;
    EXPECT_FALSE(NtpTime::decode(reply, 0, 0).ok);
}

//================================================================================
TEST(NtpTime, ChoosesTheLowestDelay) {
    NtpMeasurement slow;
    slow.ok = true;
    slow.delayUs = 400000;
    slow.offsetUs = 10;
    slow.server = QStringLiteral("slow.example");
    NtpMeasurement fast;
    fast.ok = true;
    fast.delayUs = 20000;
    fast.offsetUs = 30;
    fast.server = QStringLiteral("time.cloudflare.com");
    NtpMeasurement broken;
    broken.error = QStringLiteral("No reply from pool.ntp.org");

    const NtpMeasurement best = NtpTime::choose({slow, broken, fast});
    EXPECT_TRUE(best.ok);
    EXPECT_EQ(best.server, QStringLiteral("time.cloudflare.com"));
    EXPECT_EQ(best.offsetUs, 30);
}

//================================================================================
TEST(NtpTime, ReportsFailureWhenNobodyReplied) {
    NtpMeasurement failed;
    failed.error = QStringLiteral("down");
    const NtpMeasurement best = NtpTime::choose({failed, failed});
    EXPECT_FALSE(best.ok);
    EXPECT_EQ(best.error, QStringLiteral("No reply from time servers"));
}

//================================================================================
TEST(NtpTime, DefaultServersIncludeThePoolAndAVendor) {
    const QStringList servers = NtpTime::defaultServers();
    EXPECT_GE(servers.size(), 2);
    EXPECT_TRUE(servers.contains(QStringLiteral("pool.ntp.org")));
    EXPECT_TRUE(servers.contains(QStringLiteral("time.cloudflare.com")));
}
