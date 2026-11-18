#!/bin/bash
set -euo pipefail
# Cross-compile an Android arm64 release from Linux and pack it as an APK.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=scripts/find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

export QT_HOST_PATH="$QT_INSTALL_BASE/$QT_VERSION/$QT_HOST_DIRNAME"
QT_ANDROID_PREFIX="$QT_INSTALL_BASE/$QT_VERSION/$QT_ANDROID_DIRNAME"
export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$ANDROID_NDK_VERSION"
export QT_ANDROID_TOOLCHAIN="$QT_ANDROID_PREFIX/lib/cmake/Qt6/qt.toolchain.cmake"

if [ ! -f "$QT_ANDROID_TOOLCHAIN" ]; then
    echo "Qt Android kit not found at $QT_ANDROID_PREFIX" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Android NDK not found at $ANDROID_NDK_ROOT" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -x "$ANDROID_JAVA_HOME/bin/java" ]; then
    echo "JDK 17 not found at $ANDROID_JAVA_HOME" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
export JAVA_HOME="$ANDROID_JAVA_HOME"
export PATH="$JAVA_HOME/bin:$PATH"

# ntpclock settings win. Otherwise use the same keystore as waterfall.
KEYSTORE="${NTPCLOCK_KEYSTORE:-${WATERFALL_KEYSTORE:-$HOME/.android/waterfall-release.keystore}}"
KEY_ALIAS="${NTPCLOCK_KEY_ALIAS:-${WATERFALL_KEY_ALIAS:-waterfall}}"
KS_PASS_ENV=NTPCLOCK_KEYSTORE_PASSWORD
if [ -z "${NTPCLOCK_KEYSTORE_PASSWORD:-}" ]; then
    KS_PASS_ENV=WATERFALL_KEYSTORE_PASSWORD
fi
KEY_PASS_ENV=NTPCLOCK_KEY_PASSWORD
if [ -z "${NTPCLOCK_KEY_PASSWORD:-}" ]; then
    KEY_PASS_ENV=WATERFALL_KEY_PASSWORD
fi
APKSIGNER="$ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS/apksigner"
if [ ! -f "$KEYSTORE" ]; then
    echo "Release keystore not found at $KEYSTORE" >&2
    echo "Create it with keytool, or set NTPCLOCK_KEYSTORE or WATERFALL_KEYSTORE to its path." >&2
    exit 1
fi
if [ -z "${!KS_PASS_ENV:-}" ]; then
    echo "Set NTPCLOCK_KEYSTORE_PASSWORD or WATERFALL_KEYSTORE_PASSWORD to the keystore password." >&2
    exit 1
fi
if [ ! -x "$APKSIGNER" ]; then
    echo "apksigner not found at $APKSIGNER" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi

"$CMAKE" --preset android-release \
    -DCMAKE_PREFIX_PATH="$QT_ANDROID_PREFIX" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM"
"$CMAKE" --build --preset android-release --target apk

APK="$ROOT/build/android-release/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
if [ ! -f "$APK" ]; then
    echo "APK build finished but $APK was not produced" >&2
    exit 1
fi
VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$ROOT/build/android-release/CMakeCache.txt")"
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/ntpclock-${VERSION}-arm64-v8a.apk"
KEY_PASS_ARGS=()
if [ -n "${!KEY_PASS_ENV:-}" ]; then
    KEY_PASS_ARGS=(--key-pass "env:${KEY_PASS_ENV}")
fi
"$APKSIGNER" sign \
    --ks "$KEYSTORE" \
    --ks-key-alias "$KEY_ALIAS" \
    --ks-pass "env:${KS_PASS_ENV}" \
    "${KEY_PASS_ARGS[@]}" \
    --out "$OUT" \
    "$APK"
"$APKSIGNER" verify "$OUT" >/dev/null
rm -f "$ROOT/dist/ntpclock-${VERSION}-arm64-v8a-unsigned.apk"
echo "Signed APK:"
echo "  ${OUT##*/}"
