#include "MainWindow.h"

#include "AppIcon.h"
#include "ClockWidget.h"
#include "NetworkPermission.h"
#include "NtpService.h"
#include "ScreenAwake.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("NTP Clock"));
    AppIcon::apply(this);
    setFocusPolicy(Qt::StrongFocus);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    setAutoFillBackground(true);

    m_service = new NtpService(this);
    m_clock = new ClockWidget(this);
    m_clock->setSource([this] { return m_service->snapshot(); });
    connect(m_clock, &ClockWidget::enableRequested, this, [this] {
        showPermission(NetworkPermission::status() == NetworkPermission::Status::Blocked);
    });

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

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_clock);
    m_stack->addWidget(m_permission);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

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
        if (isFullScreen()) {
            showMaximized();
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

void MainWindow::applyPermission() {
    const NetworkPermission::Status current = NetworkPermission::status();
    if (current == NetworkPermission::Status::Granted) {
        m_service->start();
        m_stack->setCurrentWidget(m_clock);
        return;
    }
    if (current == NetworkPermission::Status::Denied) {
        m_stack->setCurrentWidget(m_clock);
        return;
    }
    showPermission(current == NetworkPermission::Status::Blocked);
}

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
