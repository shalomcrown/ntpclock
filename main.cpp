#include "AppIcon.h"
#include "MainWindow.h"
#include "ScreenAwake.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ntpclock"));
    QApplication::setApplicationName(QStringLiteral("ntpclock"));
    QApplication::setApplicationDisplayName(QStringLiteral("NTP Clock"));
    QApplication::setDesktopFileName(QStringLiteral("ntpclock"));
    QApplication::setWindowIcon(AppIcon::icon());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { ScreenAwake::setKeepAwake(false); });
    MainWindow window;
    window.showFullScreen();
    return QApplication::exec();
}
