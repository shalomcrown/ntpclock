#include "MainWindow.h"

#include "AppIcon.h"
#include "CameraInset.h"
#include "CameraPermission.h"
#include "ClockWidget.h"
#include "CompareView.h"
#include "GuideView.h"
#include "NetworkPermission.h"
#include "NtpService.h"
#include "ScreenAwake.h"
#include "TimingBlock.h"
#include "VisibleArea.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
//================================================================================
class ClockPage : public QWidget {
public:
//================================================================================
    ClockPage(ClockWidget* clock, CameraInset* inset, QWidget* parent = nullptr)
            : QWidget(parent), m_clock(clock), m_inset(inset) {
        m_clock->setParent(this);
        m_inset->setParent(this);
    }

protected:
//================================================================================
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        m_clock->setGeometry(rect());
        const int margin = 16;
        const int insetWidth = std::clamp(width() / 4, 150, 340);
        const int insetHeight = insetWidth * 3 / 4;
        m_inset->setGeometry(std::max(margin, width() - insetWidth - margin),
                             std::max(margin, height() - insetHeight - margin), insetWidth, insetHeight);
        m_inset->raise();
        m_clock->setInsetRect(m_inset->geometry());
        if (auto* guide = findChild<QPushButton*>(QStringLiteral("guideButton"))) {
            guide->setGeometry(std::max(8, width() - 96), 4, 88, 44);
            guide->raise();
        }
    }

private:
    ClockWidget* m_clock;
    CameraInset* m_inset;
};
}  // namespace

//================================================================================
MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("NTP Clock"));
    AppIcon::apply(this);
    setFocusPolicy(Qt::StrongFocus);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    setAutoFillBackground(true);

    m_service = new NtpService(this);
    m_clock = new ClockWidget();
    m_clock->setSource([this] { return m_service->snapshot(); });
    connect(m_clock, &ClockWidget::enableRequested, this, [this] {
        showPermission(NetworkPermission::status() == NetworkPermission::Status::Blocked);
    });
    m_inset = new CameraInset();
    m_inset->setPermission(CameraPermission::status());
    connect(m_inset, &CameraInset::tapped, this, &MainWindow::onCameraTapped);
    m_clockPage = new ClockPage(m_clock, m_inset, this);
    m_compare = new CompareView(this);
    connect(m_compare, &CompareView::backRequested, this, &MainWindow::showClockPage);
    m_guide = new GuideView(this);
    connect(m_guide, &GuideView::backRequested, this, &MainWindow::showClockPage);
    m_guideButton = new QPushButton(QStringLiteral("Guide"), m_clockPage);
    m_guideButton->setObjectName(QStringLiteral("guideButton"));
    m_guideButton->setFlat(true);
    m_guideButton->setCursor(Qt::PointingHandCursor);
    m_guideButton->setStyleSheet(QStringLiteral(
            "QPushButton#guideButton { background: transparent; color: #dddddd; font-size: 16px; border: none; }"));
    connect(m_guideButton, &QPushButton::clicked, this, &MainWindow::showGuide);

    m_permission = new QWidget(this);
    m_permission->setStyleSheet(QStringLiteral(
            "QWidget { background: black; color: white; }"
            "QLabel { background: black; }"
            "QPushButton { background: white; color: black; font-size: 20px; padding: 12px; }"
            "QPushButton#denyNetwork, QPushButton#retryNetwork, QPushButton#settingsNetwork {"
            " background: #222; color: white; }"));

    auto* title = new QLabel(QStringLiteral("Allow network access"), m_permission);
    title->setStyleSheet(QStringLiteral("font-size: 32px; font-weight: 600;"));
    title->setWordWrap(true);
    m_body = new QLabel(m_permission);
    m_body->setWordWrap(true);
    m_body->setStyleSheet(QStringLiteral("font-size: 18px; color: #dddddd;"));

    m_allow = new QPushButton(QStringLiteral("Allow"), m_permission);
    m_allow->setObjectName(QStringLiteral("allowNetwork"));
    m_deny = new QPushButton(QStringLiteral("Don't allow"), m_permission);
    m_deny->setObjectName(QStringLiteral("denyNetwork"));
    m_settings = new QPushButton(QStringLiteral("Open settings"), m_permission);
    m_settings->setObjectName(QStringLiteral("settingsNetwork"));
    m_retry = new QPushButton(QStringLiteral("Try again"), m_permission);
    m_retry->setObjectName(QStringLiteral("retryNetwork"));
    for (QPushButton* button : {m_allow, m_deny, m_settings, m_retry}) {
        button->setMinimumHeight(56);
    }

    auto* column = new QWidget(m_permission);
    column->setMaximumWidth(560);
    auto* columnLayout = new QVBoxLayout(column);
    columnLayout->setSpacing(16);
    columnLayout->addWidget(title);
    columnLayout->addWidget(m_body);
    columnLayout->addWidget(m_allow);
    columnLayout->addWidget(m_deny);
    columnLayout->addWidget(m_settings);
    columnLayout->addWidget(m_retry);

    auto* outer = new QVBoxLayout(m_permission);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->addStretch(1);
    auto* row = new QHBoxLayout();
    row->addStretch(1);
    row->addWidget(column, 1);
    row->addStretch(1);
    outer->addLayout(row);
    outer->addStretch(1);

    connect(m_allow, &QPushButton::clicked, this, [this] {
        NetworkPermission::remember(true);
        applyPermission();
    });
    connect(m_deny, &QPushButton::clicked, this, [this] {
        NetworkPermission::remember(false);
        applyPermission();
    });
    connect(m_settings, &QPushButton::clicked, this, [] { NetworkPermission::openSystemSettings(); });
    connect(m_retry, &QPushButton::clicked, this, &MainWindow::applyPermission);

    m_cameraPermission = new QWidget(this);
    m_cameraPermission->setObjectName(QStringLiteral("cameraPermission"));
    m_cameraPermission->setStyleSheet(QStringLiteral(
            "QWidget { background: black; color: white; }"
            "QLabel { background: black; }"
            "QPushButton { background: white; color: black; font-size: 20px; padding: 12px; }"
            "QPushButton#denyCamera, QPushButton#retryCamera, QPushButton#settingsCamera {"
            " background: #222; color: white; }"));
    auto* cameraTitle = new QLabel(QStringLiteral("Allow the camera"), m_cameraPermission);
    cameraTitle->setStyleSheet(QStringLiteral("font-size: 32px; font-weight: 600;"));
    cameraTitle->setWordWrap(true);
    m_cameraBody = new QLabel(m_cameraPermission);
    m_cameraBody->setWordWrap(true);
    m_cameraBody->setStyleSheet(QStringLiteral("font-size: 18px; color: #dddddd;"));
    m_cameraAllow = new QPushButton(QStringLiteral("Allow"), m_cameraPermission);
    m_cameraAllow->setObjectName(QStringLiteral("allowCamera"));
    m_cameraDeny = new QPushButton(QStringLiteral("Don't allow"), m_cameraPermission);
    m_cameraDeny->setObjectName(QStringLiteral("denyCamera"));
    m_cameraSettings = new QPushButton(QStringLiteral("Open settings"), m_cameraPermission);
    m_cameraSettings->setObjectName(QStringLiteral("settingsCamera"));
    m_cameraRetry = new QPushButton(QStringLiteral("Try again"), m_cameraPermission);
    m_cameraRetry->setObjectName(QStringLiteral("retryCamera"));
    for (QPushButton* button : {m_cameraAllow, m_cameraDeny, m_cameraSettings, m_cameraRetry}) {
        button->setMinimumHeight(56);
    }
    auto* cameraColumn = new QWidget(m_cameraPermission);
    cameraColumn->setMaximumWidth(560);
    auto* cameraColumnLayout = new QVBoxLayout(cameraColumn);
    cameraColumnLayout->setSpacing(16);
    cameraColumnLayout->addWidget(cameraTitle);
    cameraColumnLayout->addWidget(m_cameraBody);
    cameraColumnLayout->addWidget(m_cameraAllow);
    cameraColumnLayout->addWidget(m_cameraDeny);
    cameraColumnLayout->addWidget(m_cameraSettings);
    cameraColumnLayout->addWidget(m_cameraRetry);
    auto* cameraOuter = new QVBoxLayout(m_cameraPermission);
    cameraOuter->setContentsMargins(32, 32, 32, 32);
    cameraOuter->addStretch(1);
    auto* cameraRow = new QHBoxLayout();
    cameraRow->addStretch(1);
    cameraRow->addWidget(cameraColumn, 1);
    cameraRow->addStretch(1);
    cameraOuter->addLayout(cameraRow);
    cameraOuter->addStretch(1);
    connect(m_cameraAllow, &QPushButton::clicked, this, &MainWindow::requestCamera);
    connect(m_cameraDeny, &QPushButton::clicked, this, [this] {
        CameraPermission::remember(false);
        m_inset->setPermission(CameraPermission::status());
        showClockPage();
    });
    connect(m_cameraSettings, &QPushButton::clicked, this, [] { CameraPermission::openSystemSettings(); });
    connect(m_cameraRetry, &QPushButton::clicked, this, [this] {
        showCameraPermission(CameraPermission::status() == CameraPermission::Status::Blocked);
    });

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_clockPage);
    m_stack->addWidget(m_permission);
    m_stack->addWidget(m_cameraPermission);
    m_stack->addWidget(m_compare);
    m_stack->addWidget(m_guide);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_stack);
#if defined(Q_OS_ANDROID)
    // Android does not always tell Qt when the visible window shrinks, for
    // example after the phone turns while the app is in the background.
    auto* visibleCheck = new QTimer(this);
    connect(visibleCheck, &QTimer::timeout, this, &MainWindow::fitVisibleArea);
    visibleCheck->start(500);
#endif

    auto* quit = new QShortcut(QKeySequence::Quit, this);
    connect(quit, &QShortcut::activated, this, &QWidget::close);
    auto* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, [this] {
        if (isFullScreen()) {
            showMaximized();
        } else {
            showFullScreen();
        }
    });
    auto* leaveFullscreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(leaveFullscreen, &QShortcut::activated, this, [this] {
        if (m_stack->currentWidget() == m_compare || m_stack->currentWidget() == m_cameraPermission ||
            m_stack->currentWidget() == m_guide) {
            showClockPage();
            return;
        }
        if (isFullScreen()) {
            showMaximized();
        }
    });
    auto* shutter = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(shutter, &QShortcut::activated, this, [this] {
        if (m_stack->currentWidget() == m_clockPage) {
            onCameraTapped();
        }
    });

    if (auto* gui = qobject_cast<QGuiApplication*>(qApp)) {
        connect(gui, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive && m_stack->currentWidget() == m_permission) {
                applyPermission();
            }
        });
    }

    applyPermission();
}

//================================================================================
void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (qEnvironmentVariable("QT_QPA_PLATFORM") == QByteArrayLiteral("offscreen")) {
        return;
    }
    ScreenAwake::setKeepAwake(true);
#if defined(Q_OS_ANDROID)
    QTimer::singleShot(300, this, [] { ScreenAwake::setKeepAwake(true); });
#endif
}

//================================================================================
void MainWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    fitVisibleArea();
}

//================================================================================
void MainWindow::fitVisibleArea() {
    const QMargins margins = VisibleArea::hiddenMargins(size(), VisibleArea::devicePixels(), devicePixelRatioF());
    if (m_layout->contentsMargins() != margins) {
        m_layout->setContentsMargins(margins);
    }
}

//================================================================================
void MainWindow::applyPermission() {
    const NetworkPermission::Status current = NetworkPermission::status();
    if (current == NetworkPermission::Status::Granted) {
        m_service->start();
        showClockPage();
        return;
    }
    if (current == NetworkPermission::Status::Denied) {
        showClockPage();
        return;
    }
    m_inset->stop();
    showPermission(current == NetworkPermission::Status::Blocked);
}

//================================================================================
void MainWindow::showGuide() {
    m_inset->stop();
    m_stack->setCurrentWidget(m_guide);
}

//================================================================================
void MainWindow::showClockPage() {
    m_stack->setCurrentWidget(m_clockPage);
    m_inset->setPermission(CameraPermission::status());
    if (CameraPermission::status() == CameraPermission::Status::Granted) {
        m_inset->start();
    }
}

//================================================================================
void MainWindow::onCameraTapped() {
    if (m_stack->currentWidget() != m_clockPage) {
        return;
    }
    const CameraPermission::Status current = CameraPermission::status();
    if (current == CameraPermission::Status::Granted) {
        capture();
        return;
    }
    showCameraPermission(current == CameraPermission::Status::Blocked);
}

//================================================================================
void MainWindow::requestCamera() {
    CameraPermission::request([this](CameraPermission::Status result) {
        m_inset->setPermission(result);
        if (result == CameraPermission::Status::Granted) {
            showClockPage();
            return;
        }
        showCameraPermission(result == CameraPermission::Status::Blocked);
    });
}

//================================================================================
void MainWindow::capture() {
    const QImage photo = m_inset->currentFrame();
    if (photo.isNull()) {
        m_inset->noteMissingFrame();
        return;
    }
    // Grab the clock itself. The camera inset is a sibling, so it is not in the picture.
    const QImage clock = m_clock->grab().toImage();
    const QString latency = latencyLabel(measureLatency(photo, m_clock->timingValue(), m_clock->displayEpochMs()));
    m_inset->stop();
    m_compare->setPair(photo, clock, latency);
    m_stack->setCurrentWidget(m_compare);
}

//================================================================================
void MainWindow::showCameraPermission(bool blocked) {
    const bool systemSettings = blocked && CameraPermission::canOpenSystemSettings();
    m_cameraBody->setText(blocked ? CameraPermission::blockedText() : CameraPermission::askText());
    m_cameraAllow->setVisible(blocked == false);
    m_cameraDeny->setVisible(blocked == false);
    m_cameraSettings->setVisible(systemSettings);
    m_cameraRetry->setVisible(blocked);
    m_stack->setCurrentWidget(m_cameraPermission);
    if (blocked == false) {
        m_cameraAllow->setFocus();
    }
}

//================================================================================
void MainWindow::showPermission(bool blocked) {
    const bool systemSettings = blocked && NetworkPermission::canOpenSystemSettings();
    m_body->setText(blocked ? NetworkPermission::blockedText() : NetworkPermission::askText());
    m_allow->setVisible(blocked == false);
    m_deny->setVisible(blocked == false);
    m_settings->setVisible(systemSettings);
    m_retry->setVisible(blocked);
    m_stack->setCurrentWidget(m_permission);
    if (blocked == false) {
        m_allow->setFocus();
    }
}
