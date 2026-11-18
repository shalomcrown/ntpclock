#include "ClockWidget.h"

#include "NtpTime.h"

#include <algorithm>

#include <QFontDatabase>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

namespace {
struct Bands {
    QRect date;
    QRect time;
    QRect millis;
    QRect footer;
};

QFont fixedFont() {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    font.setBold(true);
    return font;
}

QFont fitFont(const QFont& proto, const QString& text, const QRect& band) {
    QFont font = proto;
    if (text.isEmpty() || band.width() < 2 || band.height() < 2) {
        font.setPixelSize(1);
        return font;
    }
    int low = 1;
    int high = band.height();
    int best = 1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        font.setPixelSize(mid);
        const QFontMetrics metrics(font);
        if (metrics.horizontalAdvance(text) <= band.width() && metrics.height() <= band.height()) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    font.setPixelSize(best);
    return font;
}

// The clock line is wide, so its font is limited by the window width. Give it
// only the height that font needs. The three millisecond digits get the rest,
// which makes them the largest text on a normal window.
Bands bandsFor(const QRect& area, const QFont& proto) {
    const int gap = std::max(4, area.height() / 100);
    const int dateHeight = std::clamp(area.height() * 6 / 100, 18, std::max(18, area.height() / 12));
    const int footerHeight = std::clamp(area.height() * 6 / 100, 18, std::max(18, area.height() / 12));
    const int rest = std::max(2, area.height() - dateHeight - footerHeight - gap * 3);
    const int millisFloor = std::max(1, rest * 2 / 5);
    const QFont timeFont = fitFont(proto, QStringLiteral("00:00:00"), QRect(0, 0, area.width(), rest));
    int timeHeight = QFontMetrics(timeFont).height();
    if (timeHeight > rest - millisFloor) {
        timeHeight = rest - millisFloor;
    }
    timeHeight = std::max(1, timeHeight);
    const int millisHeight = std::max(1, rest - timeHeight);

    Bands bands;
    int y = area.top();
    bands.date = QRect(area.left(), y, area.width(), dateHeight);
    y += dateHeight + gap;
    bands.time = QRect(area.left(), y, area.width(), timeHeight);
    y += timeHeight + gap;
    bands.millis = QRect(area.left(), y, area.width(), millisHeight);
    y += millisHeight + gap;
    bands.footer = QRect(area.left(), y, area.width(), footerHeight);
    return bands;
}

void drawLine(QPainter& painter, const QRect& band, const QFont& font, const QString& text) {
    painter.setFont(font);
    painter.drawText(band, Qt::AlignCenter, text);
}
}  // namespace

ClockWidget::ClockWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start(8);
}

void ClockWidget::setSource(std::function<ClockSnapshot()> source) {
    m_source = std::move(source);
    update();
}

void ClockWidget::setSnapshot(const ClockSnapshot& snapshot) {
    m_snapshot = snapshot;
    update();
}

void ClockWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (m_source) {
        m_snapshot = m_source();
    }
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (width() < 20 || height() < 20) {
        return;
    }
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const bool trustOffset = m_snapshot.networkAllowed && m_snapshot.synced;
    const qint64 displayUs = NtpTime::systemNowUs() + (trustOffset ? m_snapshot.offsetUs : 0);
    const ClockLines lines = formatClock(displayUs, m_snapshot, QTimeZone::systemTimeZone());

    const int margin = std::max(8, std::min(width(), height()) / 40);
    const QFont proto = fixedFont();
    const Bands bands = bandsFor(rect().adjusted(margin, margin, -margin, -margin), proto);
    // Fit the font to a fixed sample so the digits do not jump as the time changes.
    const QFont dateFont = fitFont(proto, QStringLiteral("0000-00-00"), bands.date);
    const QFont timeFont = fitFont(proto, QStringLiteral("00:00:00"), bands.time);
    const QFont millisFont = fitFont(proto, QStringLiteral("000"), bands.millis);
    const QFont footerFont = fitFont(proto, lines.footer, bands.footer);

    painter.setPen(QColor(210, 210, 210));
    drawLine(painter, bands.date, dateFont, lines.date);
    painter.setPen(Qt::white);
    drawLine(painter, bands.time, timeFont, lines.time);
    drawLine(painter, bands.millis, millisFont, lines.millis);
    painter.setPen(lines.warning ? QColor(255, 196, 0) : QColor(170, 170, 170));
    drawLine(painter, bands.footer, footerFont, lines.footer);
}

void ClockWidget::mousePressEvent(QMouseEvent* event) {
    if (m_snapshot.networkAllowed == false) {
        emit enableRequested();
    }
    QWidget::mousePressEvent(event);
}
