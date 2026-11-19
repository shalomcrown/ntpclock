#include "ClockWidget.h"

#include "NtpTime.h"
#include "TimingBlock.h"

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
    painter.save();
    painter.setClipRect(band);
    painter.setFont(font);
    painter.drawText(band, Qt::AlignCenter, text);
    painter.restore();
}
}  // namespace

//================================================================================
void drawFixedDigits(QPainter& painter, const QRect& band, const QFont& font, const QString& text) {
    painter.setFont(font);
    if (text.isEmpty() || band.width() < 2 || band.height() < 2) {
        return;
    }
    painter.save();
    painter.setClipRect(band);
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
    painter.restore();
}

//================================================================================
ClockWidget::ClockWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, [this] {
        if (m_layoutReady == false) {
            update();
            return;
        }
        qint64 displayUs = NtpTime::systemNowUs();
        if (m_snapshot.networkAllowed && m_snapshot.synced) {
            displayUs += m_snapshot.offsetUs;
        }
        const qint64 second = displayUs / 1000000LL;
        // The hours, date, and footer change once a second. The milliseconds
        // and the timing block change on every tick.
        if (second != m_shownSecond) {
            update(m_dateBand);
            update(m_timeBand);
            update(m_footerBand);
            update(m_millisBand);
            return;
        }
        update(m_millisBand);
    });
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
int ClockWidget::timingValue() const {
    return m_timingValue;
}

//================================================================================
qint64 ClockWidget::displayEpochMs() const {
    return m_displayEpochMs;
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
    if (m_source) {
        m_snapshot = m_source();
    }
    QPainter painter(this);
    painter.fillRect(event->rect(), Qt::black);
    if (width() < 20 || height() < 20) {
        return;
    }
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const bool trustOffset = m_snapshot.networkAllowed && m_snapshot.synced;
    const qint64 displayUs = NtpTime::systemNowUs() + (trustOffset ? m_snapshot.offsetUs : 0);
    m_timingValue = timingValueFromUs(displayUs);
    m_displayEpochMs = displayUs >= 0 ? (displayUs + 500) / 1000 : -((-displayUs + 500) / 1000);
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
        const QFontMetrics millisMetrics(m_millisFont);
        int digitCell = millisMetrics.horizontalAdvance(QStringLiteral("0"));
        if (digitCell * 3 > m_millisBand.width()) {
            digitCell = std::max(1, m_millisBand.width() / 3);
        }
        const TimingPlacement timing = placeTimingBlock(m_millisBand, digitCell * 3,
                                                       std::min(millisMetrics.height(), m_millisBand.height()), m_insetRect);
        m_millisDigitBand = timing.digits.isEmpty() ? m_millisBand : timing.digits;
        m_timingBlock = timing.block;
        if (m_millisDigitBand.width() < digitCell * 3) {
            m_millisFont = fitFont(proto, QStringLiteral("000"), m_millisDigitBand);
        }
        const QFontMetrics fitted(m_millisFont);
        m_digitCell = fitted.horizontalAdvance(QStringLiteral("0"));
        if (m_digitCell * 3 > m_millisDigitBand.width()) {
            m_digitCell = std::max(1, m_millisDigitBand.width() / 3);
        }
        m_footerFont = fitFont(proto, lines.footer, m_footerBand);
        m_layoutSize = size;
        m_layoutInset = m_insetRect;
        m_layoutFooter = lines.footer;
        m_layoutReady = true;
    } else if (m_layoutFooter != lines.footer) {
        m_footerFont = fitFont(fixedFont(), lines.footer, m_footerBand);
        m_layoutFooter = lines.footer;
    }

    const QRect exposed = event->rect();
    auto want = [&](const QRect& band) { return band.isEmpty() == false && exposed.intersects(band); };
    if (want(m_versionBand)) {
        painter.setPen(QColor(200, 200, 200));
        drawLine(painter, m_versionBand, m_versionFont, QCoreApplication::applicationVersion());
    }
    if (want(m_dateBand)) {
        painter.setPen(QColor(210, 210, 210));
        drawLine(painter, m_dateBand, m_dateFont, lines.date);
    }
    if (want(m_timeBand)) {
        painter.setPen(Qt::white);
        drawFixedDigits(painter, m_timeBand, m_timeFont, lines.time);
        // A millisecond-only repaint must not count, or the next tick will
        // skip the seconds line for as long as that paint was late.
        m_shownSecond = displayUs / 1000000LL;
    }
    if (want(m_millisBand)) {
        painter.setPen(Qt::white);
        drawMillis(painter, lines.millis);
        paintTimingBlock(painter, m_timingBlock, m_timingValue);
    }
    if (want(m_footerBand)) {
        painter.setPen(lines.warning ? QColor(255, 196, 0) : QColor(170, 170, 170));
        drawLine(painter, m_footerBand, m_footerFont, lines.footer);
    }
}

//================================================================================
void ClockWidget::drawMillis(QPainter& painter, const QString& text) {
    const int height = m_millisDigitBand.height();
    if (m_digitCell < 1 || height < 1) {
        return;
    }
    if (m_spriteFont != m_millisFont || m_spriteCell != m_digitCell || m_spriteHeight != height || m_digitSprite[0].isNull()) {
        m_spriteFont = m_millisFont;
        m_spriteCell = m_digitCell;
        m_spriteHeight = height;
        for (int digit = 0; digit < 10; ++digit) {
            QImage sprite(m_digitCell, height, QImage::Format_ARGB32_Premultiplied);
            sprite.fill(Qt::black);
            QPainter glyph(&sprite);
            glyph.setRenderHint(QPainter::TextAntialiasing, true);
            glyph.setPen(Qt::white);
            glyph.setFont(m_millisFont);
            glyph.drawText(sprite.rect(), Qt::AlignCenter, QString::number(digit));
            m_digitSprite[digit] = sprite;
        }
    }
    painter.save();
    painter.setClipRect(m_millisDigitBand);
    int cell = m_digitCell;
    if (cell * static_cast<int>(text.size()) > m_millisDigitBand.width()) {
        cell = std::max(1, m_millisDigitBand.width() / std::max(1, static_cast<int>(text.size())));
    }
    const int row = cell * static_cast<int>(text.size());
    int x = m_millisDigitBand.left() + (m_millisDigitBand.width() - row) / 2;
    for (const QChar character : text) {
        const int digit = character.digitValue();
        if (digit >= 0 && digit <= 9) {
            painter.drawImage(QRect(x, m_millisDigitBand.top(), cell, m_millisDigitBand.height()), m_digitSprite[digit]);
        }
        x += cell;
    }
    painter.restore();
}

//================================================================================
void ClockWidget::mousePressEvent(QMouseEvent* event) {
    if (m_snapshot.networkAllowed == false) {
        emit enableRequested();
    }
    QWidget::mousePressEvent(event);
}
