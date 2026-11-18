#include "NtpWorker.h"

#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <gtest/gtest.h>

TEST(NtpWorker, LiveSync) {
    if (qEnvironmentVariableIsEmpty("NTPCLOCK_LIVE_NTP")) {
        GTEST_SKIP() << "Set NTPCLOCK_LIVE_NTP=1 to query the time servers";
    }

    NtpWorker worker;
    QThread thread;
    worker.moveToThread(&thread);
    thread.start();

    QEventLoop loop;
    bool finished = false;
    bool ok = false;
    QString server;
    QString error;
    QObject::connect(&worker, &NtpWorker::syncFinished, &loop,
                     [&](bool success, qint64, qint64, const QString& usedServer, const QString& failure) {
                         finished = true;
                         ok = success;
                         server = usedServer;
                         error = failure;
                         loop.quit();
                     });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    QMetaObject::invokeMethod(&worker, &NtpWorker::syncNow);
    loop.exec();

    thread.quit();
    thread.wait(5000);
    ASSERT_TRUE(finished) << "timed out waiting for NTP";
    EXPECT_TRUE(ok) << error.toStdString();
    EXPECT_FALSE(server.isEmpty());
}
