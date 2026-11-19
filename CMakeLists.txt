cmake_minimum_required(VERSION 4.4)
project(ntpclock VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)
if(UNIX AND NOT ANDROID AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

find_package(Git QUIET)
set(NTPCLOCK_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(NTPCLOCK_VERSION_HEADER "${NTPCLOCK_GENERATED_DIR}/ntpclock_version.h")
add_custom_target(ntpclock_version
        COMMAND "${CMAKE_COMMAND}"
                "-DBASE=${PROJECT_VERSION}"
                "-DSRC=${CMAKE_SOURCE_DIR}"
                "-DOUT=${NTPCLOCK_VERSION_HEADER}"
                "-DGIT=${GIT_EXECUTABLE}"
                -P "${CMAKE_SOURCE_DIR}/scripts/write-version.cmake"
        BYPRODUCTS "${NTPCLOCK_VERSION_HEADER}"
        VERBATIM
)

set(NTPCLOCK_SOURCES
        src/VisibleArea.cpp
        src/TimingCode.cpp
        src/TimingBlock.cpp
        src/DigitRead.cpp
        src/MainWindow.cpp
        src/ClockWidget.cpp
        src/ClockFormat.cpp
        src/NtpService.cpp
        src/NtpWorker.cpp
        src/NtpTime.cpp
        src/NetworkPermission.cpp
        src/CameraPermission.cpp
        src/CameraInset.cpp
        src/CompareView.cpp
        src/GuideView.cpp
        src/GuideText.cpp
        src/PowerEvents.cpp
        src/ScreenAwake.cpp
        src/AppIcon.cpp
)

qt_add_executable(ntpclock
        main.cpp
        ${NTPCLOCK_SOURCES}
        resources/guide.qrc
)
set_target_properties(ntpclock PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)
if(WIN32)
    set_target_properties(ntpclock PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_include_directories(ntpclock PRIVATE src "${NTPCLOCK_GENERATED_DIR}")
add_dependencies(ntpclock ntpclock_version)
if(ANDROID)
    qt_policy(SET QTP0002 NEW)
    find_program(NTPCLOCK_IMAGEMAGICK NAMES convert magick REQUIRED)
    set(NTPCLOCK_ANDROID_PACKAGE_DIR "${CMAKE_BINARY_DIR}/android-package")
    set(NTPCLOCK_ANDROID_ICON "${NTPCLOCK_ANDROID_PACKAGE_DIR}/res/mipmap-xxxhdpi/ic_launcher.png")
    add_custom_command(
            OUTPUT "${NTPCLOCK_ANDROID_ICON}"
            COMMAND "${CMAKE_COMMAND}"
                    "-DIM=${NTPCLOCK_IMAGEMAGICK}"
                    "-DSVG=${CMAKE_SOURCE_DIR}/resources/ntpclock.svg"
                    "-DSRC=${CMAKE_SOURCE_DIR}/android"
                    "-DOUT=${NTPCLOCK_ANDROID_PACKAGE_DIR}"
                    -P "${CMAKE_SOURCE_DIR}/scripts/render-android-icons.cmake"
            DEPENDS
                    "${CMAKE_SOURCE_DIR}/scripts/render-android-icons.cmake"
                    "${CMAKE_SOURCE_DIR}/resources/ntpclock.svg"
                    "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml"
                    "${CMAKE_SOURCE_DIR}/android/res/drawable/ic_launcher_foreground.xml"
                    "${CMAKE_SOURCE_DIR}/android/res/mipmap-anydpi-v26/ic_launcher.xml"
                    "${CMAKE_SOURCE_DIR}/android/res/mipmap-anydpi-v26/ic_launcher_round.xml"
                    "${CMAKE_SOURCE_DIR}/android/res/values/colors.xml"
            VERBATIM
    )
    add_custom_target(android_icons DEPENDS "${NTPCLOCK_ANDROID_ICON}")
    set_target_properties(ntpclock PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${NTPCLOCK_ANDROID_PACKAGE_DIR}"
            QT_ANDROID_PACKAGE_NAME "org.qtproject.example.ntpclock"
            QT_ANDROID_TARGET_SDK_VERSION 36
            QT_ANDROID_MIN_SDK_VERSION 28
    )
    # NDK r27 aligns to 4 KB unless the link asks for 16 KB pages.
    # https://doc.qt.io/qt-6.11/android.html
    target_link_options(ntpclock PRIVATE "-Wl,-z,max-page-size=16384")
    cmake_language(DEFER CALL add_dependencies ntpclock_make_apk android_icons)
    cmake_language(DEFER CALL add_dependencies ntpclock_make_aab android_icons)
endif()
target_link_libraries(ntpclock PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Multimedia
)
if(TARGET Qt6::DBus)
    target_link_libraries(ntpclock PRIVATE Qt6::DBus)
    target_compile_definitions(ntpclock PRIVATE NTPCLOCK_HAS_DBUS=1)
endif()
if(WIN32)
    target_link_libraries(ntpclock PRIVATE ws2_32)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(ntpclock_tests
            tests/main.cpp
            tests/ntp_time_test.cpp
            tests/clock_format_test.cpp
            tests/network_permission_test.cpp
            tests/camera_permission_test.cpp
            tests/camera_frame_test.cpp
            tests/compare_view_test.cpp
            tests/guide_test.cpp
            tests/clock_widget_test.cpp
            tests/main_window_test.cpp
            tests/ntp_live_test.cpp
            tests/version_test.cpp
            tests/timing_block_test.cpp
            tests/digit_read_test.cpp
            ${NTPCLOCK_SOURCES}
            resources/guide.qrc
    )
    target_include_directories(ntpclock_tests PRIVATE src "${NTPCLOCK_GENERATED_DIR}")
    target_compile_definitions(ntpclock_tests PRIVATE NTPCLOCK_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    add_dependencies(ntpclock_tests ntpclock_version)
    set_target_properties(ntpclock_tests PROPERTIES AUTOMOC ON AUTORCC ON)
    target_link_libraries(ntpclock_tests PRIVATE
            GTest::gtest
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            Qt::Multimedia
    )
    if(TARGET Qt6::DBus)
        target_link_libraries(ntpclock_tests PRIVATE Qt6::DBus)
        target_compile_definitions(ntpclock_tests PRIVATE NTPCLOCK_HAS_DBUS=1)
    endif()
    include(GoogleTest)
    gtest_discover_tests(ntpclock_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(
            NAME build_scripts
            COMMAND bash "${CMAKE_SOURCE_DIR}/tests/build_scripts_test.sh"
    )
endif()

if(ANDROID)
    return()
endif()

include(GNUInstallDirs)

set(APP_ID "ntpclock")
set(APP_DISPLAY_NAME "NTP Clock")
set(APP_EXE_NAME "ntpclock")

install(TARGETS ntpclock
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    if(NOT QT_TARGET_PREFIX AND Qt6_DIR)
        get_filename_component(_qt6_dir "${Qt6_DIR}" DIRECTORY)
        get_filename_component(_qt6_dir "${_qt6_dir}" DIRECTORY)
        get_filename_component(QT_TARGET_PREFIX "${_qt6_dir}" DIRECTORY)
    endif()
    if(NOT EXISTS "${QT_TARGET_PREFIX}/bin/Qt6Core.dll")
        message(FATAL_ERROR
                "Qt Windows DLLs not found under '${QT_TARGET_PREFIX}'. "
                "Run ./install-prereqs.sh --mode windows")
    endif()

    file(GLOB _qt_dlls "${QT_TARGET_PREFIX}/bin/*.dll")
    install(FILES ${_qt_dlls} DESTINATION ${CMAKE_INSTALL_BINDIR})
    foreach(_plugin platforms styles imageformats iconengines multimedia)
        if(EXISTS "${QT_TARGET_PREFIX}/plugins/${_plugin}")
            install(DIRECTORY "${QT_TARGET_PREFIX}/plugins/${_plugin}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    endforeach()

    foreach(_dll_name libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${_dll_name}
                OUTPUT_VARIABLE _dll_path
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(EXISTS "${_dll_path}")
            install(FILES "${_dll_path}" DESTINATION ${CMAKE_INSTALL_BINDIR})
        else()
            message(FATAL_ERROR
                    "MinGW runtime DLL not found: ${_dll_name}. Install mingw-w64.")
        endif()
    endforeach()
endif()

if(UNIX AND NOT APPLE)
    configure_file(
            "${CMAKE_SOURCE_DIR}/resources/ntpclock.desktop.in"
            "${CMAKE_BINARY_DIR}/${APP_ID}.desktop"
            @ONLY
    )
    install(FILES "${CMAKE_BINARY_DIR}/${APP_ID}.desktop"
            DESTINATION share/applications
    )
    install(FILES "${CMAKE_SOURCE_DIR}/resources/ntpclock.svg"
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME ntpclock.svg
    )
    file(WRITE "${CMAKE_BINARY_DIR}/postinst"
"#!/bin/sh
set -e
update-desktop-database -q || true
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor || true
fi
exit 0
")
    file(WRITE "${CMAKE_BINARY_DIR}/postrm"
"#!/bin/sh
set -e
update-desktop-database -q || true
exit 0
")
    execute_process(COMMAND chmod +755 "${CMAKE_BINARY_DIR}/postinst" "${CMAKE_BINARY_DIR}/postrm")
endif()

set(CPACK_PACKAGE_NAME "${APP_ID}")
set(CPACK_PACKAGE_VENDOR "ntpclock")
set(CPACK_PACKAGE_CONTACT "ntpclock@localhost")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NTP clock for measuring video latency")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${APP_DISPLAY_NAME}")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_MONOLITHIC_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "${APP_DISPLAY_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${APP_DISPLAY_NAME}")
    set(CPACK_NSIS_CONTACT "ntpclock@localhost")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_INSTALL_ALL_USERS ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${APP_EXE_NAME}.exe")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
"DetailPrint 'Stopping running ${APP_DISPLAY_NAME} instances (if any)...'
nsExec::ExecToLog 'taskkill /F /IM ${APP_EXE_NAME}.exe'
Sleep 500"
    )
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
"CreateDirectory '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}'
CreateShortCut '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}\\\\${APP_DISPLAY_NAME}.lnk' '\$INSTDIR\\\\bin\\\\${APP_EXE_NAME}.exe'
CreateShortCut '\$DESKTOP\\\\${APP_DISPLAY_NAME}.lnk' '\$INSTDIR\\\\bin\\\\${APP_EXE_NAME}.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
"Delete '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}\\\\${APP_DISPLAY_NAME}.lnk'
RMDir '\$SMPROGRAMS\\\\${APP_DISPLAY_NAME}'
Delete '\$DESKTOP\\\\${APP_DISPLAY_NAME}.lnk'"
    )
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "NTP Clock Maintainers <ntpclock@localhost>")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_BINARY_DIR}/postinst;${CMAKE_BINARY_DIR}/postrm"
    )
endif()

include(CPack)
