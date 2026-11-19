#include "VisibleArea.h"
#include "ntpclock_version.h"

#include <QRegularExpression>
#include <QString>

#include <gtest/gtest.h>

//================================================================================
TEST(Version, HasBuildTimeAndGitHash) {
    const QString version = QStringLiteral(NTPCLOCK_VERSION);
    const QRegularExpression shape(QStringLiteral("^\\d+\\.\\d+\\.\\d+\\.\\d{12}\\.([0-9a-f]{4,}|nogit)$"));
    EXPECT_TRUE(shape.match(version).hasMatch()) << version.toStdString();
}

//================================================================================
TEST(VisibleArea, TrimsTheWidgetToTheVisiblePixels) {
    // Portrait Pixel 10 Pro XL: Qt sized the window to the whole 2404 px screen,
    // but Android kept it below the 161 px camera cutout.
    const QMargins margins = VisibleArea::hiddenMargins(QSize(443, 986), QSize(1080, 2243), 2.4375);
    EXPECT_EQ(margins, QMargins(0, 0, 0, 66));
}

//================================================================================
TEST(VisibleArea, TrimsTheRightSideInLandscape) {
    const QMargins margins = VisibleArea::hiddenMargins(QSize(986, 443), QSize(2243, 1080), 2.4375);
    EXPECT_EQ(margins, QMargins(0, 0, 66, 0));
}

//================================================================================
TEST(VisibleArea, LeavesAFittingWidgetAlone) {
    EXPECT_EQ(VisibleArea::hiddenMargins(QSize(443, 920), QSize(1080, 2243), 2.4375), QMargins());
    EXPECT_EQ(VisibleArea::hiddenMargins(QSize(800, 600), QSize(), 1.0), QMargins());
}
