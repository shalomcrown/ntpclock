#!/bin/bash
set -euo pipefail
# Cross-compile an Android arm64 release from Linux.
# The APK installs over USB. The Android App Bundle is what Play accepts.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=scripts/find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

export QT_HOST_PATH="$QT_INSTALL_BASE/$QT_ANDROID_VERSION/$QT_HOST_DIRNAME"
QT_ANDROID_PREFIX="$QT_INSTALL_BASE/$QT_ANDROID_VERSION/$QT_ANDROID_DIRNAME"
export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$ANDROID_NDK_VERSION"
export QT_ANDROID_TOOLCHAIN="$QT_ANDROID_PREFIX/lib/cmake/Qt6/qt.toolchain.cmake"

if [ ! -f "$QT_ANDROID_TOOLCHAIN" ]; then
    echo "Qt Android kit not found at $QT_ANDROID_PREFIX" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -x "$QT_HOST_PATH/bin/androiddeployqt" ]; then
    echo "androiddeployqt not found at $QT_HOST_PATH/bin/androiddeployqt" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Android NDK not found at $ANDROID_NDK_ROOT" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -x "$ANDROID_JAVA_HOME/bin/java" ]; then
    echo "JDK 21 not found at $ANDROID_JAVA_HOME" >&2
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
ZIPALIGN="$ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS/zipalign"
AAPT="$ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS/aapt"
JARSIGNER="$JAVA_HOME/bin/jarsigner"
READELF="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
if [ ! -f "$KEYSTORE" ]; then
    echo "Release keystore not found at $KEYSTORE" >&2
    echo "Create it with keytool, or set NTPCLOCK_KEYSTORE or WATERFALL_KEYSTORE to its path." >&2
    exit 1
fi
if [ -z "${!KS_PASS_ENV:-}" ]; then
    echo "Set NTPCLOCK_KEYSTORE_PASSWORD or WATERFALL_KEYSTORE_PASSWORD to the keystore password." >&2
    exit 1
fi
if [ ! -x "$APKSIGNER" ] || [ ! -x "$ZIPALIGN" ] || [ ! -x "$AAPT" ]; then
    echo "Android build-tools not found at $ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -x "$JARSIGNER" ]; then
    echo "jarsigner not found at $JARSIGNER" >&2
    exit 1
fi
if [ ! -x "$READELF" ]; then
    echo "llvm-readelf not found at $READELF" >&2
    exit 1
fi

#================================================================================
stop_gradle_daemon() {
    local gradlew="$ROOT/build/android-release/android-build/gradlew"
    if [ -x "$gradlew" ]; then
        (cd "$(dirname "$gradlew")" && ./gradlew --stop) || true
    fi
}

#================================================================================
drop_stale_android_tree() {
    local cache="$ROOT/build/android-release/CMakeCache.txt"
    if [ -f "$cache" ] && ! grep -q "$QT_ANDROID_VERSION" "$cache"; then
        echo "Android build tree was configured for a different Qt. Removing it."
        rm -rf "$ROOT/build/android-release"
    fi
}

#================================================================================
pick_newest_package() {
    local pattern="$1" prefer_unsigned="$2"
    local match pick=""
    mapfile -t matches < <(find "$ROOT/build/android-release" -path "$pattern" -type f -printf '%T@ %p\n' | sort -nr | cut -d' ' -f2-)
    if [ "${#matches[@]}" -eq 0 ]; then
        echo "No package matching $pattern" >&2
        exit 1
    fi
    if [ "$prefer_unsigned" = "yes" ]; then
        for match in "${matches[@]}"; do
            case "$match" in
                *unsigned*) pick="$match"; break ;;
            esac
        done
    fi
    if [ -z "$pick" ]; then
        pick="${matches[0]}"
    fi
    printf '%s\n' "$pick"
}

#================================================================================
require_elf_page_size() {
    local package="$1"
    local tmp so aligns align found=0
    tmp="$(mktemp -d)"
    unzip -qo "$package" -d "$tmp"
    while IFS= read -r so; do
        found=1
        aligns="$("$READELF" -W -l "$so" | awk '$1=="LOAD" { print $NF }')"
        if [ -z "$aligns" ]; then
            echo "No LOAD segment in ${so#"$tmp"/}" >&2
            rm -rf "$tmp"
            exit 1
        fi
        for align in $aligns; do
            if [ "$((align))" -lt "$((0x4000))" ]; then
                echo "Native library ${so#"$tmp"/} is aligned to $align, which is below 16 KB" >&2
                rm -rf "$tmp"
                exit 1
            fi
        done
    done < <(find "$tmp" -type f -name '*.so')
    rm -rf "$tmp"
    if [ "$found" -eq 0 ]; then
        echo "No native libraries in ${package##*/}" >&2
        exit 1
    fi
}

#================================================================================
require_play_sdk_levels() {
    local apk="$1" badging
    badging="$("$AAPT" dump badging "$apk")"
    if ! printf '%s\n' "$badging" | grep -q "targetSdkVersion:'36'"; then
        echo "APK target SDK is not 36" >&2
        printf '%s\n' "$badging" | grep -E 'sdkVersion|targetSdk' >&2 || true
        exit 1
    fi
    if ! printf '%s\n' "$badging" | grep -q "sdkVersion:'28'"; then
        echo "APK minimum SDK is not 28" >&2
        printf '%s\n' "$badging" | grep -E 'sdkVersion|targetSdk' >&2 || true
        exit 1
    fi
}

stop_gradle_daemon
drop_stale_android_tree

"$CMAKE" --preset android-release \
    -DCMAKE_PREFIX_PATH="$QT_ANDROID_PREFIX" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_PLATFORM="$ANDROID_NATIVE_PLATFORM"
"$CMAKE" --build --preset android-release --target apk
"$CMAKE" --build --preset android-release --target aab

APK="$(pick_newest_package '*/outputs/apk/release/*.apk' yes)"
AAB="$(pick_newest_package '*/outputs/bundle/release/*.aab' yes)"
VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$ROOT/build/android-release/CMakeCache.txt")"
mkdir -p "$ROOT/dist"
APK_OUT="$ROOT/dist/ntpclock-${VERSION}-arm64-v8a.apk"
AAB_OUT="$ROOT/dist/ntpclock-${VERSION}-arm64-v8a.aab"
KEY_PASS_ARGS=()
if [ -n "${!KEY_PASS_ENV:-}" ]; then
    KEY_PASS_ARGS=(--key-pass "env:${KEY_PASS_ENV}")
fi
"$APKSIGNER" sign \
    --ks "$KEYSTORE" \
    --ks-key-alias "$KEY_ALIAS" \
    --ks-pass "env:${KS_PASS_ENV}" \
    "${KEY_PASS_ARGS[@]}" \
    --out "$APK_OUT" \
    "$APK"
"$APKSIGNER" verify "$APK_OUT" >/dev/null
rm -f "$ROOT/dist/ntpclock-${VERSION}-arm64-v8a-unsigned.apk"

JAR_ARGS=(
    -sigalg SHA256withRSA
    -digestalg SHA-256
    -keystore "$KEYSTORE"
    -storepass:env "$KS_PASS_ENV"
)
if [ -n "${!KEY_PASS_ENV:-}" ]; then
    JAR_ARGS+=(-keypass:env "$KEY_PASS_ENV")
fi
rm -f "$AAB_OUT"
"$JARSIGNER" "${JAR_ARGS[@]}" -signedjar "$AAB_OUT" "$AAB" "$KEY_ALIAS"
"$JARSIGNER" -verify "$AAB_OUT" >/dev/null

"$ZIPALIGN" -c -P 16 4 "$APK_OUT"
"$ZIPALIGN" -c -P 16 4 "$AAB_OUT"
require_elf_page_size "$APK_OUT"
require_elf_page_size "$AAB_OUT"
require_play_sdk_levels "$APK_OUT"

echo "Signed APK:"
echo "  ${APK_OUT##*/}"
echo "Signed App Bundle:"
echo "  ${AAB_OUT##*/}"
