#include "NtpTime.h"

#include <chrono>

namespace {
constexpr qint64 kNtpUnixDeltaSeconds = 2208988800LL;
constexpr qint64 kMaxDelayUs = 3000000;

//================================================================================
quint32 read32(const char* data) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    return (quint32(bytes[0]) << 24) | (quint32(bytes[1]) << 16) | (quint32(bytes[2]) << 8) | quint32(bytes[3]);
}

//================================================================================
void write32(char* data, quint32 value) {
    auto* bytes = reinterpret_cast<unsigned char*>(data);
    bytes[0] = static_cast<unsigned char>((value >> 24) & 0xff);
    bytes[1] = static_cast<unsigned char>((value >> 16) & 0xff);
    bytes[2] = static_cast<unsigned char>((value >> 8) & 0xff);
    bytes[3] = static_cast<unsigned char>(value & 0xff);
}

//================================================================================
void writeTimestamp(char* dest, qint64 unixUs) {
    qint64 seconds = unixUs / 1000000;
    qint64 micros = unixUs % 1000000;
    if (micros < 0) {
        seconds -= 1;
        micros += 1000000;
    }
    const quint32 ntpSeconds = static_cast<quint32>(seconds + kNtpUnixDeltaSeconds);
    const quint32 fraction = static_cast<quint32>((micros << 32) / 1000000);
    write32(dest, ntpSeconds);
    write32(dest + 4, fraction);
}

//================================================================================
qint64 readUnixUs(const char* data) {
    const quint32 ntpSeconds = read32(data);
    const quint32 fraction = read32(data + 4);
    const qint64 unixSeconds = static_cast<qint64>(ntpSeconds) - kNtpUnixDeltaSeconds;
    const qint64 micros = (static_cast<qint64>(fraction) * 1000000LL) >> 32;
    return unixSeconds * 1000000LL + micros;
}

//================================================================================
NtpMeasurement failed(const QString& error) {
    NtpMeasurement result;
    result.error = error;
    return result;
}
}  // namespace

//================================================================================
qint64 NtpTime::systemNowUs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

//================================================================================
QStringList NtpTime::defaultServers() {
    // Vendor servers first. pool.ntp.org is the public aggregate, queried with them.
    return {
            QStringLiteral("time.cloudflare.com"),
            QStringLiteral("time.google.com"),
            QStringLiteral("time.windows.com"),
            QStringLiteral("pool.ntp.org"),
    };
}

//================================================================================
QByteArray NtpTime::requestPacket(qint64 t1Us) {
    QByteArray packet(kPacketBytes, 0);
    packet[0] = 0x23;  // LI = 0, version = 4, mode = client
    writeTimestamp(packet.data() + 40, t1Us);
    return packet;
}

//================================================================================
NtpMeasurement NtpTime::fromExchange(qint64 t1Us, qint64 t2Us, qint64 t3Us, qint64 t4Us) {
    NtpMeasurement result;
    const qint64 delayUs = (t4Us - t1Us) - (t3Us - t2Us);
    if (delayUs < 0 || delayUs > kMaxDelayUs) {
        result.error = QStringLiteral("Unusable NTP reply");
        return result;
    }
    result.ok = true;
    result.delayUs = delayUs;
    result.offsetUs = ((t2Us - t1Us) + (t3Us - t4Us)) / 2;
    return result;
}

//================================================================================
NtpMeasurement NtpTime::decode(const QByteArray& packet, qint64 t1Us, qint64 t4Us) {
    if (packet.size() < kPacketBytes) {
        return failed(QStringLiteral("Short NTP packet"));
    }
    const unsigned char first = static_cast<unsigned char>(packet[0]);
    const int leap = first >> 6;
    const int mode = first & 0x7;
    const int stratum = static_cast<unsigned char>(packet[1]);
    if (leap == 3) {
        return failed(QStringLiteral("NTP server is unsynchronized"));
    }
    if (mode != 4 || stratum < 1 || stratum > 15) {
        return failed(QStringLiteral("Unexpected NTP packet"));
    }
    const QByteArray expected = requestPacket(t1Us);
    if (packet.mid(24, 8) != expected.mid(40, 8)) {
        return failed(QStringLiteral("NTP reply does not match the query"));
    }
    const qint64 t2Us = readUnixUs(packet.constData() + 32);
    const qint64 t3Us = readUnixUs(packet.constData() + 40);
    return fromExchange(t1Us, t2Us, t3Us, t4Us);
}

//================================================================================
NtpMeasurement NtpTime::choose(const QList<NtpMeasurement>& samples) {
    const NtpMeasurement* best = nullptr;
    for (const NtpMeasurement& sample : samples) {
        if (sample.ok == false) {
            continue;
        }
        if (best == nullptr || sample.delayUs < best->delayUs) {
            best = &sample;
        }
    }
    if (best != nullptr) {
        return *best;
    }
    if (samples.size() == 1 && samples.first().error.isEmpty() == false) {
        return failed(samples.first().error);
    }
    return failed(QStringLiteral("No reply from time servers"));
}
