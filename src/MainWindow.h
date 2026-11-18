#pragma once

#include <QWidget>

class CameraInset;
class ClockWidget;
class CompareView;
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
    void showCameraPermission(bool blocked);
    void onCameraTapped();
    void requestCamera();
    void capture();
    void showClockPage();

    NtpService* m_service = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_clockPage = nullptr;
    ClockWidget* m_clock = nullptr;
    CameraInset* m_inset = nullptr;
    CompareView* m_compare = nullptr;
    QWidget* m_cameraPermission = nullptr;
    QLabel* m_cameraBody = nullptr;
    QPushButton* m_cameraAllow = nullptr;
    QPushButton* m_cameraDeny = nullptr;
    QPushButton* m_cameraSettings = nullptr;
    QPushButton* m_cameraRetry = nullptr;
    QWidget* m_permission = nullptr;
    QLabel* m_body = nullptr;
    QPushButton* m_allow = nullptr;
    QPushButton* m_deny = nullptr;
    QPushButton* m_settings = nullptr;
    QPushButton* m_retry = nullptr;
};
