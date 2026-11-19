#include "CompareView.h"

#include <algorithm>

#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>

//================================================================================
CompareView::CompareView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("compareView"));
    setAutoFillBackground(false);
    m_back = new QPushButton(QStringLiteral("Back"), this);
    m_back->setObjectName(QStringLiteral("compareBack"));
    m_back->setMinimumHeight(56);
    m_back->setStyleSheet(QStringLiteral("QPushButton { background: white; color: black; font-size: 20px; }"));
    connect(m_back, &QPushButton::clicked, this, &CompareView::backRequested);
}

//================================================================================
void CompareView::setPair(const QImage& otherScreen, const QImage& clock) {
    m_other = otherScreen;
    m_clock = clock;
    update();
}

//================================================================================
void CompareView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const int width = std::min(220, this->width() - 32);
    m_back->setGeometry((this->width() - width) / 2, height() - 72, width, 56);
}

//================================================================================
void CompareView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_back->setFocus();
}

//================================================================================
void CompareView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    const QRect area = rect().adjusted(0, 0, 0, -80);
    const QRect left(area.left(), area.top(), area.width() / 2, area.height());
    const QRect right(left.right() + 1, area.top(), area.width() - left.width(), area.height());
    drawPane(painter, left, m_other, QStringLiteral("Other screen"));
    drawPane(painter, right, m_clock, QStringLiteral("This clock"));
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawLine(left.right(), area.top(), left.right(), area.bottom());
}

//================================================================================
void CompareView::drawPane(QPainter& painter, const QRect& pane, const QImage& image, const QString& caption) const {
    painter.setPen(QColor(210, 210, 210));
    const QRect title(pane.left(), pane.top(), pane.width(), 36);
    painter.drawText(title, Qt::AlignCenter, caption);
    const QRect area = pane.adjusted(12, 40, -12, -12);
    if (image.isNull() || area.width() < 2 || area.height() < 2) {
        return;
    }
    const QSize fitted = image.size().scaled(area.size(), Qt::KeepAspectRatio);
    const QRect dest(area.center().x() - fitted.width() / 2, area.center().y() - fitted.height() / 2, fitted.width(),
                     fitted.height());
    painter.drawImage(dest, image);
}
