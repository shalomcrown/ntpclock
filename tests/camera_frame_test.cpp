#include "CameraInset.h"

#include <QColor>
#include <QImage>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <gtest/gtest.h>

namespace {
//================================================================================
QImage marked() {
    QImage image(2, 2, QImage::Format_RGB32);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(1, 0, Qt::green);
    image.setPixelColor(0, 1, Qt::blue);
    image.setPixelColor(1, 1, Qt::white);
    return image;
}
}  // namespace

//================================================================================
TEST(CameraFrame, QuarterTurnMovesTheCorners) {
    const QImage turned = orientCameraImage(marked(), 90, false);
    ASSERT_EQ(turned.size(), QSize(2, 2));
    EXPECT_EQ(turned.pixelColor(1, 0), QColor(Qt::red));
    EXPECT_EQ(turned.pixelColor(1, 1), QColor(Qt::green));
    EXPECT_EQ(turned.pixelColor(0, 0), QColor(Qt::blue));
    EXPECT_EQ(turned.pixelColor(0, 1), QColor(Qt::white));
}

//================================================================================
TEST(CameraFrame, HalfTurnAndMirror) {
    const QImage turned = orientCameraImage(marked(), 180, false);
    EXPECT_EQ(turned.pixelColor(1, 1), QColor(Qt::red));
    EXPECT_EQ(turned.pixelColor(0, 0), QColor(Qt::white));

    const QImage flipped = orientCameraImage(marked(), 0, true);
    EXPECT_EQ(flipped.pixelColor(1, 0), QColor(Qt::red));
    EXPECT_EQ(flipped.pixelColor(0, 0), QColor(Qt::green));
}

//================================================================================
TEST(CameraFrame, LeavesAnEmptyImageEmpty) {
    EXPECT_TRUE(orientCameraImage(QImage(), 90, true).isNull());
}

//================================================================================
// The inset relies on toImage() applying the frame's rotation. Turning the
// image again left the portrait preview on the phone sideways.
TEST(CameraFrame, ToImageAlreadyAppliesTheFrameRotation) {
    QVideoFrame frame(QVideoFrameFormat(QSize(2, 1), QVideoFrameFormat::Format_BGRX8888));
    ASSERT_TRUE(frame.map(QVideoFrame::WriteOnly));
    auto* pixels = reinterpret_cast<quint32*>(frame.bits(0));
    pixels[0] = 0xffff0000u;
    pixels[1] = 0xff0000ffu;
    frame.unmap();
    frame.setRotationAngle(QVideoFrame::Rotation90);

    const QImage image = frame.toImage();
    ASSERT_EQ(image.size(), QSize(1, 2));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(Qt::red));
    EXPECT_EQ(image.pixelColor(0, 1), QColor(Qt::blue));
}
