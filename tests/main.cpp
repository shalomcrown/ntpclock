#include "ntpclock_version.h"

#include <QApplication>
#include <QSettings>

#include <gtest/gtest.h>

//================================================================================
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ntpclock-test"));
    QApplication::setApplicationName(QStringLiteral("ntpclock-test"));
    QApplication::setApplicationVersion(QStringLiteral(NTPCLOCK_VERSION));
    QSettings settings;
    settings.clear();
    settings.sync();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
