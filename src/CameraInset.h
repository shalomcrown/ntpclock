#pragma once

#include "CameraPermission.h"

#include <QImage>
#include <QVideoFrame>
#include <QWidget>

class QCamera;
class QMediaCaptureSession;
class QVideoSink;
class QWidget;

//================================================================================
// Upright copy of a camera image. degrees is clockwise.
QImage orientCameraImage(const QImage& image, int degrees, bool mirrored);

//================================================================================
// Small live view of the camera, used to frame the other screen.
class CameraInset : public QWidget {
    Q_OBJECT

public:
//================================================================================
    explicit CameraInset(QWidget* parent = nullptr);

//================================================================================
    void setPermission(CameraPermission::Status status);
//================================================================================
    void setNote(const QString& note);
//================================================================================
    // Used when the shutter fires before a frame has arrived. Keeps a more
    // specific note, such as "No camera", when one is already showing.
    void noteMissingFrame();
//================================================================================
    QImage currentFrame() const;
//================================================================================
    void start();
//================================================================================
    void stop();

signals:
//================================================================================
    void tapped();

protected:
//================================================================================
    void paintEvent(QPaintEvent* event) override;
//================================================================================
    void mousePressEvent(QMouseEvent* event) override;
//================================================================================
    void resizeEvent(QResizeEvent* event) override;

private:
//================================================================================
    void ensureCamera();
//================================================================================
    void toggleFacing();
//================================================================================
    void updateFacingSwitch();
//================================================================================
    void rememberFrame(const QImage& image, bool frameMirrored);
//================================================================================
    void enqueueFrame(const QVideoFrame& frame);
//================================================================================
    void convertAway(const QVideoFrame& frame);
//================================================================================
    void finishConversion(const QImage& image, bool frameMirrored);

    CameraPermission::Status m_permission = CameraPermission::Status::Undetermined;
    QString m_note;
    QImage m_frame;
    QImage m_source;
    bool m_mirrored = false;
    bool m_front = false;
    bool m_running = false;
    bool m_converting = false;
    bool m_hasPending = false;
    QVideoFrame m_pending;
    qint64 m_previewShownAt = 0;
    QWidget* m_facingSwitch = nullptr;
    QCamera* m_camera = nullptr;
    QMediaCaptureSession* m_session = nullptr;
    QVideoSink* m_sink = nullptr;
};
