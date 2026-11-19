#include "ClockWidget.h"

#include "NtpTime.h"

#include <algorithm>

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

namespace {
//================================================================================
struct Bands {
    QRect version;
    QRect date;
    QRect time;
    QRect millis;
    QRect footer;
};

//================================================================================
QFont fixedFont() {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    font.setBold(true);
    return font;
}

//================================================================================
QFont fitFont(const QFont& proto, const QString& text, const QRect& band) {
    QFont font = proto;
    if (text.isEmpty() || band.width() < 2 || band.height() < 2) {
        font.setPixelSize(1);
        return font;
    }
    // A font taller than the band, or wider than one character's share of the
    // band, cannot fit. Portrait leaves a very tall millisecond band, and
    // probing a font that tall on every frame stalls the clock.
    const int characters = std::max(1, static_cast<int>(text.size()));
    const int perCharacter = std::max(1, band.width() / characters);
    int low = 1;
    int high = std::min(band.height(), perCharacter * 3);
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

//================================================================================
// The clock line is wide, so its font is limited by the window width. Give it
// only the height that font needs. The three millisecond digits get the rest,
// which makes them the largest text on a normal window.
Bands bandsFor(const QRect& area, const QFont& proto) {
    const int gap = std::max(4, area.height() / 100);
    const int versionHeight = std::clamp(area.height() / 40, 14, 22);
    const int dateHeight = std::clamp(area.height() * 6 / 100, 18, std::max(18, area.height() / 12));
    const int footerHeight = std::clamp(area.height() * 6 / 100, 18, std::max(18, area.height() / 12));
    const int rest = std::max(2, area.height() - versionHeight - dateHeight - footerHeight - gap * 4);
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
    bands.version = QRect(area.left(), y, area.width(), versionHeight);
    y += versionHeight + gap;
    bands.date = QRect(area.left(), y, area.width(), dateHeight);
    y += dateHeight + gap;
    bands.time = QRect(area.left(), y, area.width(), timeHeight);
    y += timeHeight + gap;
    bands.millis = QRect(area.left(), y, area.width(), millisHeight);
    y += millisHeight + gap;
    bands.footer = QRect(area.left(), y, area.width(), footerHeight);
    return bands;
}

//================================================================================
void drawLine(QPainter& painter, const QRect& band, const QFont& font, const QString& text) {
    painter.setFont(font);
    painter.drawText(band, Qt::AlignCenter, text);
}
}  // namespace

//================================================================================
void drawFixedDigits(QPainter& painter, const QRect& band, const QFont& font, const QString& text) {
    painter.setFont(font);
    if (text.isEmpty() || band.width() < 2 || band.height() < 2) {
        return;
    }
    const QFontMetrics metrics(font);
    int cell = std::max(1, metrics.horizontalAdvance(QStringLiteral("0")));
    if (cell * text.size() > band.width()) {
        cell = std::max(1, band.width() / static_cast<int>(text.size()));
    }
    const int row = cell * text.size();
    int x = band.left() + (band.width() - row) / 2;
    for (const QChar character : text) {
        painter.drawText(QRect(x, band.top(), cell, band.height()), Qt::AlignCenter, QString(character));
        x += cell;
    }
}

//================================================================================
ClockWidget::ClockWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start(8);
}

//================================================================================
void ClockWidget::setSource(std::function<ClockSnapshot()> source) {
    m_source = std::move(source);
    update();
}

//================================================================================
void ClockWidget::setSnapshot(const ClockSnapshot& snapshot) {
    m_snapshot = snapshot;
    update();
}

//================================================================================
void ClockWidget::setInsetRect(const QRect& rect) {
    if (m_insetRect == rect) {
        return;
    }
    m_insetRect = rect;
    update();
}

//================================================================================
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

    const QSize size(width(), height());
    if (m_layoutReady == false || m_layoutSize != size || m_layoutInset != m_insetRect) {
        const int margin = std::max(8, std::min(width(), height()) / 40);
        const QFont proto = fixedFont();
        Bands bands = bandsFor(rect().adjusted(margin, margin, -margin, -margin), proto);
        const auto clearOfInset = [this](const QRect& band) {
            const QRect blocked = m_insetRect.adjusted(0, -12, 0, 0);
            if (m_insetRect.isEmpty() || band.intersects(blocked) == false) {
                return band;
            }
            const int right = m_insetRect.left() - 8;
            if (right <= band.left() + 40) {
                return band;
            }
            return QRect(band.left(), band.top(), right - band.left(), band.height());
        };
        m_versionBand = bands.version;
        m_dateBand = clearOfInset(bands.date);
        m_timeBand = clearOfInset(bands.time);
        m_millisBand = clearOfInset(bands.millis);
        m_footerBand = clearOfInset(bands.footer);
        // Fit the font to a fixed sample so the digits do not jump as the time changes.
        m_versionFont = proto;
        m_versionFont.setBold(false);
        m_versionFont.setPixelSize(std::clamp(m_versionBand.height() - 4, 11, 18));
        m_dateFont = fitFont(proto, QStringLiteral("0000-00-00"), m_dateBand);
        m_timeFont = fitFont(proto, QStringLiteral("00:00:00"), m_timeBand);
        m_millisFont = fitFont(proto, QStringLiteral("000"), m_millisBand);
        m_footerFont = fitFont(proto, lines.footer, m_footerBand);
        m_layoutSize = size;
        m_layoutInset = m_insetRect;
        m_layoutFooter = lines.footer;
        m_layoutReady = true;
    } else if (m_layoutFooter != lines.footer) {
        m_footerFont = fitFont(fixedFont(), lines.footer, m_footerBand);
        m_layoutFooter = lines.footer;
    }

    painter.setPen(QColor(200, 200, 200));
    drawLine(painter, m_versionBand, m_versionFont, QCoreApplication::applicationVersion());
    painter.setPen(QColor(210, 210, 210));
    drawLine(painter, m_dateBand, m_dateFont, lines.date);
    painter.setPen(Qt::white);
    drawFixedDigits(painter, m_timeBand, m_timeFont, lines.time);
    drawFixedDigits(painter, m_millisBand, m_millisFont, lines.millis);
    painter.setPen(lines.warning ? QColor(255, 196, 0) : QColor(170, 170, 170));
    drawLine(painter, m_footerBand, m_footerFont, lines.footer);
}

//================================================================================
void ClockWidget::mousePressEvent(QMouseEvent* event) {
    if (m_snapshot.networkAllowed == false) {
        emit enableRequested();
    }
    QWidget::mousePressEvent(event);
}
