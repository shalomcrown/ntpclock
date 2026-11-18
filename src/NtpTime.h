#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// One NTP exchange. offsetUs is added to the system clock to get NTP time.
// A positive offset means the system clock is behind the server.
struct NtpMeasurement {
    bool ok = false;
    qint64 offsetUs = 0;
    qint64 delayUs = 0;
    QString server;
    QString error;
};

// SNTP (RFC 4330) packet codec and the offset/delay calculation. No sockets.
class NtpTime {
public:
    static constexpr int kPacketBytes = 48;

    static qint64 systemNowUs();
    static QStringList defaultServers();
    static QByteArray requestPacket(qint64 t1Us);
    static NtpMeasurement decode(const QByteArray& packet, qint64 t1Us, qint64 t4Us);
    static NtpMeasurement fromExchange(qint64 t1Us, qint64 t2Us, qint64 t3Us, qint64 t4Us);
    static NtpMeasurement choose(const QList<NtpMeasurement>& samples);
};
