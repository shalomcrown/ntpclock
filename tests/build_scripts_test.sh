#!/bin/bash
# The packaging scripts stay wired to the prerequisite pins and the CPack generators.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

#================================================================================
require_line() {
    local file="$1" pattern="$2"
    if ! grep -q -- "$pattern" "$file"; then
        echo "missing '$pattern' in $file" >&2
        exit 1
    fi
}

for script in install-prereqs.sh build-linux.sh build-windows.sh build-android.sh install-apk.sh upload-firebase.sh scripts/run-tests.sh; do
    head -n 2 "$ROOT/$script" | grep -q 'set -euo pipefail' || {
        echo "$script must enable set -euo pipefail" >&2
        exit 1
    }
done

require_line "$ROOT/install-prereqs.sh" "scripts/prereqs.env"
require_line "$ROOT/install-prereqs.sh" "qt6-base-dev"
require_line "$ROOT/install-prereqs.sh" "qt6-multimedia-dev"
require_line "$ROOT/install-prereqs.sh" "qtmultimedia"
require_line "$ROOT/install-prereqs.sh" "dpkg-dev"
require_line "$ROOT/install-prereqs.sh" "mingw-w64"
require_line "$ROOT/install-prereqs.sh" "nsis"
require_line "$ROOT/install-prereqs.sh" "aqtinstall"
require_line "$ROOT/install-prereqs.sh" "openjdk-21-jdk"
require_line "$ROOT/install-prereqs.sh" "all_os"
require_line "$ROOT/install-prereqs.sh" "imagemagick"
require_line "$ROOT/install-prereqs.sh" "ANDROID_NDK_VERSION"
require_line "$ROOT/scripts/prereqs.env" "QT_VERSION=6.6.3"
require_line "$ROOT/scripts/prereqs.env" "QT_ANDROID_VERSION=6.11.3"
require_line "$ROOT/scripts/prereqs.env" "QT_ANDROID_HOST_ARCH=linux_gcc_64"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_NDK_VERSION=27.2.12479018"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_PLATFORM=android-36"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_NATIVE_PLATFORM=android-35"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_BUILD_TOOLS=36.0.0"

require_line "$ROOT/build-linux.sh" "scripts/prereqs.env"
require_line "$ROOT/build-linux.sh" "--preset linux-release"
require_line "$ROOT/build-linux.sh" "--target package"
require_line "$ROOT/build-windows.sh" "--preset windows-mingw-release"
require_line "$ROOT/build-windows.sh" "--target package"
require_line "$ROOT/build-android.sh" "--preset android-release"
require_line "$ROOT/build-android.sh" "--target apk"
require_line "$ROOT/build-android.sh" "--target aab"
require_line "$ROOT/build-android.sh" "QT_ANDROID_VERSION"
require_line "$ROOT/build-android.sh" "ANDROID_NATIVE_PLATFORM"
require_line "$ROOT/build-android.sh" "jarsigner"
require_line "$ROOT/build-android.sh" "0x4000"
require_line "$ROOT/build-android.sh" "targetSdkVersion:'36'"
require_line "$ROOT/build-android.sh" "sdkVersion:'28'"
require_line "$ROOT/install-apk.sh" "scripts/prereqs.env"
require_line "$ROOT/install-apk.sh" "adb"
require_line "$ROOT/install-apk.sh" "install -r"
require_line "$ROOT/install-apk.sh" "cannot open the USB device"
require_line "$ROOT/install-prereqs.sh" "51-android.rules"
require_line "$ROOT/install-prereqs.sh" "https://firebase.tools"
require_line "$ROOT/install-prereqs.sh" "ensure_firebase"
require_line "$ROOT/upload-firebase.sh" "appdistribution:distribute"
require_line "$ROOT/upload-firebase.sh" "1:309763775369:android:f3ca73101d839f988c9f58"
require_line "$ROOT/upload-firebase.sh" "FIREBASE_APP_ID"
require_line "$ROOT/upload-firebase.sh" "unsigned"
require_line "$ROOT/upload-firebase.sh" "No tester group"
require_line "$ROOT/upload-firebase.sh" "appdistribution:testers:list"
require_line "$ROOT/upload-firebase.sh" "GROUP_ALIASES"
if grep -E -q '(^|[[:space:]])GROUPS=' "$ROOT/upload-firebase.sh"; then
    echo "upload-firebase.sh must not assign GROUPS; bash uses it for numeric group ids" >&2
    exit 1
fi
if grep -E -q '^(export )?FIREBASE_TOKEN=' "$ROOT/upload-firebase.sh" "$ROOT/scripts/prereqs.env" "$ROOT/install-prereqs.sh"; then
    echo "Firebase token must come from the environment, not the repo" >&2
    exit 1
fi
require_line "$ROOT/build-android.sh" 'export JAVA_HOME="$ANDROID_JAVA_HOME"'
require_line "$ROOT/build-android.sh" "apksigner"
require_line "$ROOT/build-android.sh" "waterfall-release.keystore"
require_line "$ROOT/build-android.sh" "WATERFALL_KEYSTORE_PASSWORD"
require_line "$ROOT/build-android.sh" "WATERFALL_KEY_ALIAS"
if grep -E -q '^(export )?(NTPCLOCK|WATERFALL)_(KEYSTORE|KEY)_PASSWORD=' "$ROOT/build-android.sh" "$ROOT/scripts/prereqs.env"; then
    echo "keystore password must come from the environment, not the repo" >&2
    exit 1
fi
require_line "$ROOT/scripts/prereqs.env" "ANDROID_JAVA_HOME=.*java-21"

require_line "$ROOT/resources/ntpclock.desktop.in" "Icon=ntpclock"
require_line "$ROOT/resources/ntpclock.desktop.in" "StartupWMClass=ntpclock"
require_line "$ROOT/resources/ntpclock.svg" "<svg"
require_line "$ROOT/CMakeLists.txt" "share/icons/hicolor/scalable/apps"
require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "NSIS")'
require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "DEB")'
require_line "$ROOT/CMakeLists.txt" "if(ANDROID)"
require_line "$ROOT/CMakeLists.txt" "render-android-icons.cmake"
require_line "$ROOT/CMakeLists.txt" "QT_ANDROID_TARGET_SDK_VERSION 36"
require_line "$ROOT/CMakeLists.txt" "QT_ANDROID_MIN_SDK_VERSION 28"
require_line "$ROOT/CMakeLists.txt" "max-page-size=16384"
require_line "$ROOT/CMakeLists.txt" "ntpclock_make_aab"
require_line "$ROOT/android/AndroidManifest.xml" "android:icon=\"@mipmap/ic_launcher\""
require_line "$ROOT/android/AndroidManifest.xml" "android.permission.INTERNET"
require_line "$ROOT/android/AndroidManifest.xml" "android.permission.CAMERA"
require_line "$ROOT/android/res/mipmap-anydpi-v26/ic_launcher.xml" "adaptive-icon"

if grep -q 'windows-mingw' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Windows preset" >&2
    exit 1
fi
if grep -q 'android-release' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Android preset" >&2
    exit 1
fi

echo "build_scripts_test: ok"
