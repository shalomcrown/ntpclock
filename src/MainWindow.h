#pragma once

#include <QWidget>

class ClockWidget;
class NtpService;
class QLabel;
class QPushButton;
class QStackedWidget;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void applyPermission();
    void showPermission(bool blocked);

    NtpService* m_service = nullptr;
    QStackedWidget* m_stack = nullptr;
    ClockWidget* m_clock = nullptr;
    QWidget* m_permission = nullptr;
    QLabel* m_body = nullptr;
    QPushButton* m_allow = nullptr;
    QPushButton* m_deny = nullptr;
    QPushButton* m_settings = nullptr;
    QPushButton* m_retry = nullptr;
};
