#include "AppIcon.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QWidget>

namespace {
QPixmap drawIcon(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal radius = size * 0.22;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0));
    painter.drawRoundedRect(QRectF(0, 0, size, size), radius, radius);

    const qreal margin = size * 0.18;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, size * 0.06));
    painter.drawEllipse(QRectF(margin, margin, size - margin * 2, size - margin * 2));
    painter.setPen(QPen(Qt::white, size * 0.055, Qt::SolidLine, Qt::RoundCap));
    const QPointF center(size / 2.0, size / 2.0);
    painter.drawLine(center, QPointF(center.x(), size * 0.30));
    painter.setPen(QPen(Qt::white, size * 0.04, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, QPointF(size * 0.70, center.y()));
    return pixmap;
}
}  // namespace

QIcon AppIcon::icon() {
    QIcon icon;
    icon.addPixmap(drawIcon(64));
    icon.addPixmap(drawIcon(128));
    icon.addPixmap(drawIcon(256));
    return icon;
}

void AppIcon::apply(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setWindowIcon(icon());
}
