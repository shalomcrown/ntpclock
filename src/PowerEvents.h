#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

// Wakes the clock after suspend, sleep, or the app returning from the background.
class PowerEvents : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit PowerEvents(QObject* parent = nullptr);
    ~PowerEvents() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void resumed();

private slots:
    void onPrepareForSleep(bool sleeping);

private:
    bool m_asleep = false;
};
