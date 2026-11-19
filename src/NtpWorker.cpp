#include "NtpWorker.h"

#include "NtpTime.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QUdpSocket>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace {
constexpr int kTimeoutMs = 1200;
constexpr quint16 kNtpPort = 123;

//================================================================================
QList<QHostAddress> lookup(const QString& host, QString& error) {
    QList<QHostAddress> addresses;
    addrinfo hints{};
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    addrinfo* resolved = nullptr;
    const QByteArray name = host.toUtf8();
    const int code = getaddrinfo(name.constData(), "123", &hints, &resolved);
    if (code != 0) {
        error = QString::fromLocal8Bit(gai_strerror(code));
        return addresses;
    }
    for (addrinfo* entry = resolved; entry != nullptr; entry = entry->ai_next) {
        if (entry->ai_addr == nullptr) {
            continue;
        }
        addresses.append(QHostAddress(entry->ai_addr));
    }
    freeaddrinfo(resolved);
    if (addresses.isEmpty()) {
        error = QStringLiteral("No address for %1").arg(host);
    }
    return addresses;
}

//================================================================================
QHostAddress preferIpv4(const QList<QHostAddress>& addresses) {
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            return address;
        }
    }
    return addresses.isEmpty() ? QHostAddress() : addresses.first();
}

//================================================================================
NtpMeasurement queryServer(const QString& server) {
    NtpMeasurement result;
    result.server = server;
    QString lookupError;
    const QList<QHostAddress> addresses = lookup(server, lookupError);
    const QHostAddress address = preferIpv4(addresses);
    if (address.isNull()) {
        result.error = lookupError.isEmpty() ? QStringLiteral("Could not resolve %1").arg(server) : lookupError;
        return result;
    }

    const qint64 t1Us = NtpTime::systemNowUs();
    const QByteArray packet = NtpTime::requestPacket(t1Us);
    QUdpSocket socket;
    const QHostAddress bindAddress = address.protocol() == QAbstractSocket::IPv6Protocol ? QHostAddress::AnyIPv6
                                                                                          : QHostAddress::AnyIPv4;
    socket.bind(bindAddress, 0);
    if (socket.writeDatagram(packet, address, kNtpPort) != packet.size()) {
        result.error = QStringLiteral("Could not send to %1").arg(server);
        return result;
    }

    result.error = QStringLiteral("No reply from %1").arg(server);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kTimeoutMs) {
        const int remaining = kTimeoutMs - static_cast<int>(timer.elapsed());
        if (socket.waitForReadyRead(remaining) == false) {
            continue;
        }
        while (socket.hasPendingDatagrams()) {
            QByteArray reply;
            reply.resize(static_cast<int>(socket.pendingDatagramSize()));
            const qint64 t4Us = NtpTime::systemNowUs();
            socket.readDatagram(reply.data(), reply.size());
            NtpMeasurement decoded = NtpTime::decode(reply, t1Us, t4Us);
            decoded.server = server;
            if (decoded.ok) {
                return decoded;
            }
            result = decoded;
        }
    }
    return result;
}
}  // namespace

//================================================================================
void NtpWorker::syncNow() {
    QList<NtpMeasurement> samples;
    const QStringList servers = NtpTime::defaultServers();
    samples.reserve(servers.size());
    for (const QString& server : servers) {
        samples.append(queryServer(server));
    }
    const NtpMeasurement best = NtpTime::choose(samples);
    if (best.ok) {
        const qint64 ntpUs = NtpTime::systemNowUs() + best.offsetUs;
        qInfo().noquote() << QStringLiteral("NTP %1 delay %2 ms, offset %3 ms")
                                     .arg(best.server)
                                     .arg(best.delayUs / 1000)
                                     .arg(best.offsetUs / 1000);
        emit syncFinished(true, best.offsetUs, ntpUs, best.server, QString());
        return;
    }
    qWarning().noquote() << QStringLiteral("NTP sync failed:") << best.error;
    emit syncFinished(false, 0, 0, QString(), best.error);
}
