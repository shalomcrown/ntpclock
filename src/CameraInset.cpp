#include "CameraInset.h"

#include <QCamera>
#include <QCameraDevice>
#include <QDateTime>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QThreadPool>
#include <QTransform>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {
//================================================================================
int normalizeDegrees(int degrees) {
    return ((degrees % 360) + 360) % 360;
}

//================================================================================
QCameraDevice cameraWithPosition(QCameraDevice::Position position) {
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice& camera : cameras) {
        if (camera.position() == position) {
            return camera;
        }
    }
    return {};
}

//================================================================================
QCameraDevice chooseCamera(bool front) {
    const QCameraDevice::Position position = front ? QCameraDevice::FrontFace : QCameraDevice::BackFace;
    const QCameraDevice preferred = cameraWithPosition(position);
    if (preferred.isNull() == false) {
        return preferred;
    }
    if (front) {
        return {};
    }
    const QCameraDevice fallback = QMediaDevices::defaultVideoInput();
    if (fallback.isNull() == false) {
        return fallback;
    }
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    return cameras.isEmpty() ? QCameraDevice() : cameras.first();
}

//================================================================================
QCameraFormat chooseFormat(const QCameraDevice& device) {
    QCameraFormat best;
    int bestArea = 0;
    QCameraFormat largest;
    int largestArea = 0;
    for (const QCameraFormat& format : device.videoFormats()) {
        const QSize size = format.resolution();
        const int area = size.width() * size.height();
        if (area > largestArea) {
            largest = format;
            largestArea = area;
        }
        const int longSide = std::max(size.width(), size.height());
        if (longSide <= 1920 && area > bestArea) {
            best = format;
            bestArea = area;
        }
    }
    return best.isNull() ? largest : best;
}

//================================================================================
class FacingSwitch : public QWidget {
public:
    std::function<void()> pressed;

//================================================================================
    explicit FacingSwitch(QWidget* parent) : QWidget(parent) {
        setObjectName(QStringLiteral("cameraFacing"));
        setAccessibleName(QStringLiteral("Switch camera"));
        setCursor(Qt::PointingHandCursor);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFixedSize(36, 36);
    }

protected:
//================================================================================
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = QRectF(rect()).adjusted(3, 7, -3, -7);
        const int start = 45 * 16;
        const int span = 270 * 16;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 190), 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawArc(box, start, span);
        painter.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawArc(box, start, span);

        const double end = (45.0 + 270.0) * std::acos(-1.0) / 180.0;
        const QPointF center = box.center();
        const double rx = box.width() / 2.0;
        const double ry = box.height() / 2.0;
        const QPointF tip(center.x() + rx * std::cos(end), center.y() - ry * std::sin(end));
        QPointF direction(-rx * std::sin(end), -ry * std::cos(end));
        const double length = std::hypot(direction.x(), direction.y());
        if (length < 1.0) {
            return;
        }
        direction /= length;
        const QPointF side(-direction.y(), direction.x());
        constexpr double head = 6.5;
        QPolygonF arrow;
        arrow << tip + direction * 1.2 << tip - direction * head + side * (head * 0.55)
              << tip - direction * head - side * (head * 0.55);
        painter.setPen(QPen(QColor(0, 0, 0, 190), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::white);
        painter.drawPolygon(arrow);
    }

//================================================================================
    void mousePressEvent(QMouseEvent* event) override {
        if (pressed) {
            pressed();
        }
        event->accept();
    }
};

//================================================================================
QImage previewCopy(const QImage& image) {
    constexpr int kPreviewLongSide = 640;
    if (std::max(image.width(), image.height()) <= kPreviewLongSide) {
        return image;
    }
    return image.scaled(kPreviewLongSide, kPreviewLongSide, Qt::KeepAspectRatio, Qt::FastTransformation);
}
}  // namespace

//================================================================================
CameraInset::CameraInset(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("cameraInset"));
    setAccessibleName(QStringLiteral("Camera preview"));
    setAccessibleDescription(
            QStringLiteral("Tap to photograph the other screen. Space does the same. Selfie switches to the front camera."));
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    auto* facing = new FacingSwitch(this);
    facing->setFocusPolicy(Qt::NoFocus);
    facing->pressed = [this] { toggleFacing(); };
    m_facingSwitch = facing;
    updateFacingSwitch();
}

//================================================================================
void CameraInset::setNote(const QString& note) {
    m_note = note;
    update();
}

//================================================================================
void CameraInset::noteMissingFrame() {
    if (m_note.isEmpty()) {
        setNote(QStringLiteral("No picture yet"));
    }
}

//================================================================================
void CameraInset::setPermission(CameraPermission::Status status) {
    m_permission = status;
    if (status != CameraPermission::Status::Granted) {
        stop();
        m_frame = QImage();
        m_source = QImage();
    }
    update();
}

//================================================================================
QImage CameraInset::currentFrame() const {
    return orientCameraImage(m_source, 0, m_mirrored);
}

//================================================================================
void CameraInset::start() {
    if (m_permission != CameraPermission::Status::Granted) {
        return;
    }
    m_running = true;
    ensureCamera();
    if (m_camera == nullptr) {
        update();
        return;
    }
    m_camera->start();
    update();
}

//================================================================================
void CameraInset::stop() {
    m_running = false;
    if (m_camera != nullptr) {
        m_camera->stop();
    }
}

//================================================================================
void CameraInset::updateFacingSwitch() {
    if (m_front) {
        m_facingSwitch->setAccessibleName(QStringLiteral("Switch to the back camera"));
    } else {
        m_facingSwitch->setAccessibleName(QStringLiteral("Switch to the selfie camera"));
    }
}

//================================================================================
void CameraInset::toggleFacing() {
    const bool nextFront = m_front == false;
    const QCameraDevice device = chooseCamera(nextFront);
    if (device.isNull()) {
        m_facingSwitch->setAccessibleName(nextFront ? QStringLiteral("This phone has no selfie camera")
                                                    : QStringLiteral("This phone has no back camera"));
        return;
    }
    m_front = nextFront;
    m_note.clear();
    m_frame = QImage();
    m_source = QImage();
    updateFacingSwitch();
    if (m_camera == nullptr) {
        update();
        return;
    }
    m_camera->stop();
    m_camera->setCameraDevice(device);
    const QCameraFormat format = chooseFormat(device);
    if (format.isNull() == false) {
        m_camera->setCameraFormat(format);
    }
    if (m_running) {
        m_camera->start();
    }
    update();
}

//================================================================================
void CameraInset::rememberFrame(const QImage& image, bool frameMirrored) {
    // QVideoFrame::toImage() already applies the frame's rotation and mirror,
    // so the image is upright for the way the phone is held. Turning it again
    // left the portrait preview sideways.
    m_source = image;
    // A selfie of this screen in a mirror only reads normally when the picture
    // is mirrored. Mirror the front camera once, unless Qt already did.
    m_mirrored = m_front && frameMirrored == false;
    m_note.clear();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_frame.isNull() || now - m_previewShownAt >= 50) {
        m_previewShownAt = now;
        m_frame = orientCameraImage(previewCopy(m_source), 0, m_mirrored);
        update();
    }
}

//================================================================================
void CameraInset::enqueueFrame(const QVideoFrame& frame) {
    if (frame.isValid() == false) {
        return;
    }
    if (m_converting) {
        m_pending = frame;
        m_hasPending = true;
        return;
    }
    m_converting = true;
    convertAway(frame);
}

//================================================================================
void CameraInset::convertAway(const QVideoFrame& frame) {
    // toImage() of a 1920-square camera frame takes long enough to stall the
    // clock if it runs on the UI thread. The copy keeps the buffer alive until
    // the worker finishes.
    const bool frameMirrored = frame.mirrored();
    const QPointer<CameraInset> alive(this);
    QThreadPool::globalInstance()->start([alive, frame, frameMirrored] {
        const QImage image = frame.toImage();
        if (alive.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
                alive.data(),
                [alive, image, frameMirrored] {
                    if (alive.isNull()) {
                        return;
                    }
                    alive->finishConversion(image, frameMirrored);
                },
                Qt::QueuedConnection);
    });
}

//================================================================================
void CameraInset::finishConversion(const QImage& image, bool frameMirrored) {
    if (image.isNull() == false) {
        rememberFrame(image, frameMirrored);
    }
    if (m_hasPending) {
        const QVideoFrame next = m_pending;
        m_hasPending = false;
        m_pending = QVideoFrame();
        convertAway(next);
        return;
    }
    m_converting = false;
}

//================================================================================
void CameraInset::ensureCamera() {
    if (m_camera != nullptr) {
        return;
    }
    const QCameraDevice device = chooseCamera(m_front);
    if (device.isNull()) {
        m_note = m_front ? QStringLiteral("No selfie camera") : QStringLiteral("No camera");
        return;
    }
    const QCameraFormat format = chooseFormat(device);
    m_camera = new QCamera(device, this);
    if (format.isNull() == false) {
        m_camera->setCameraFormat(format);
    }
    m_sink = new QVideoSink(this);
    m_session = new QMediaCaptureSession(this);
    m_session->setCamera(m_camera);
    m_session->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) { enqueueFrame(frame); });
    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString& text) {
        m_note = text;
        update();
    });
}

//================================================================================
QImage orientCameraImage(const QImage& image, int degrees, bool mirrored) {
    if (image.isNull()) {
        return image;
    }
    QImage result = image;
    const int turns = normalizeDegrees(degrees);
    if (turns != 0) {
        QTransform transform;
        transform.rotate(turns);
        result = result.transformed(transform);
    }
    if (mirrored) {
        result = result.mirrored(true, false);
    }
    return result;
}

//================================================================================
void CameraInset::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(rect().adjusted(1, 1, -2, -2));

    if (m_frame.isNull() == false) {
        const QRect area = rect().adjusted(4, 4, -4, -4);
        const QSize fitted = m_frame.size().scaled(area.size(), Qt::KeepAspectRatio);
        const QRect dest(area.center().x() - fitted.width() / 2, area.center().y() - fitted.height() / 2,
                         fitted.width(), fitted.height());
        painter.drawImage(dest, m_frame);
        painter.setPen(QPen(Qt::white, 3));
        painter.setBrush(Qt::NoBrush);
        const int radius = std::clamp(std::min(width(), height()) / 8, 14, 28);
        painter.drawEllipse(QPoint(width() / 2, height() - radius - 10), radius, radius);
        return;
    }

    QString text = m_note;
    if (text.isEmpty()) {
        if (m_permission == CameraPermission::Status::Blocked) {
            text = QStringLiteral("Camera blocked");
        } else if (m_permission == CameraPermission::Status::Denied) {
            text = QStringLiteral("Camera off");
        } else if (m_permission == CameraPermission::Status::Granted) {
            text = QStringLiteral("Starting camera");
        } else {
            text = QStringLiteral("Allow camera");
        }
    }
    painter.setPen(Qt::white);
    painter.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
}

//================================================================================
void CameraInset::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const int margin = 6;
    m_facingSwitch->move(std::max(margin, width() - m_facingSwitch->width() - margin), margin);
    m_facingSwitch->raise();
}

//================================================================================
void CameraInset::mousePressEvent(QMouseEvent* event) {
    emit tapped();
    QWidget::mousePressEvent(event);
}
