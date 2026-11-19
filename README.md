# NTP Clock

NTP Clock is a small Qt application that shows the time in a font large enough to film. It is meant for measuring video latency: put it on a screen, photograph that screen, and read the milliseconds.

The date is on the top line. Hours, minutes, and seconds fill the next line. Milliseconds are alone on the line under that, so those three digits can be as large as the window allows. The time of the last successful NTP sync is along the bottom, with the server that answered and how long ago it was.

The clock shows local time, corrected by the NTP offset. Two devices in the same time zone show the same clock. The screen stays on while the app is open.

The app can photograph the other screen and show that picture beside this clock. That comparison is how the delay is read.

It builds for Linux (DEB), Windows (NSIS installer, cross-compiled from Linux), and Android (arm64 APK).

## Using it

The app opens full screen. The first launch asks before any time server is contacted. **Allow** starts NTP. **Don't allow** shows the computer's own clock, and the bottom line says the time is not from NTP. Tap the clock to be asked again.

On Android, internet access is granted when the app is installed. If that access is later turned off, the app explains why it is needed and opens the system settings screen.

NTP runs on a background thread, separate from the display. It runs at startup, every 5 minutes, and after the computer wakes from suspend or the app returns from sleep. Each sync asks these servers and keeps the reply with the shortest round trip:

| Server | What it is |
| --- | --- |
| time.cloudflare.com | Cloudflare time |
| time.google.com | Google time |
| time.windows.com | Windows time |
| pool.ntp.org | NTP pool aggregate |

The bottom line is amber until a sync has succeeded, and again if a later sync fails. The digits keep using the last good offset. Wait until the bottom line shows a time before using the clock to measure latency.

Esc leaves full screen. F11 toggles it. Ctrl+Q quits. Esc from the comparison, or from the camera question, returns to the clock instead.

### Photographing another screen

A camera inset sits in the bottom-right corner. The time and milliseconds move aside so the inset does not cover them. Point the inset at the other screen, then tap it or press Space.

The first use asks for the camera. On Android that is the system permission screen. **Don't allow** leaves the clock up. The inset stays tappable, so the question can be asked again.

The comparison puts the photograph on the left and this clock on the right, both from the moment the picture was taken. **Back** or Esc returns to the live clock. The picture is kept only for that comparison. It is not saved and it is not sent anywhere.

The right-hand clock is the time at the shutter. The digits in the photograph were already on the other screen, so this clock can read later. The difference is the other screen's delay plus the delay of this camera.

A block of squares sits with the milliseconds. It is a Gray code for the millisecond inside a four-second window, so a blurred photo is off by at most one millisecond. The comparison reads that block and shows how far behind the other screen is. If the block cannot be read, the pictures stay up and the line says so.

## Build

The scripts expect to be run from the repository root on Linux. CMake 4.4 or newer is required. `install-prereqs.sh` installs it when the system copy is older.

Linux uses the distribution Qt (`qt6-base-dev` and `qt6-multimedia-dev`). Windows and Android use Qt 6.6.3 from [aqtinstall](https://github.com/miurahr/aqtinstall), pinned in `scripts/prereqs.env`. The Windows and Android kits also need the Qt Multimedia module.

### Prerequisites

```bash
./install-prereqs.sh                  # Linux, Windows cross tools, and Android
./install-prereqs.sh --mode linux
./install-prereqs.sh --mode windows
./install-prereqs.sh --mode android
./install-prereqs.sh --dry-run
```

`--mode android` also installs a udev rule so adb can open a phone that is plugged in. Unplug and replug the phone after that.

### Packages

```bash
./build-linux.sh       # dist/ntpclock-<version>-Linux.deb
./build-windows.sh     # dist/ntpclock-<version>-win64.exe
./build-android.sh     # dist/ntpclock-<version>-arm64-v8a.apk
```

The Linux package installs a desktop entry and icon, so the app shows up in the Ubuntu launcher and sidebar. The Android launcher icon is the same artwork. `install-prereqs.sh --mode android` installs ImageMagick, which the Android build uses to turn that SVG into the launcher sizes.

### Run from a build tree

```bash
./scripts/run-tests.sh
./build/linux-debug/ntpclock
```

`./build-linux.sh` leaves a release binary at `build/linux-release/ntpclock`.

### Tests

```bash
./scripts/run-tests.sh
```

`./build-linux.sh` runs the same tests against the release build before it packs the DEB. The tests do not contact a time server. To check a live query:

```bash
NTPCLOCK_LIVE_NTP=1 ./build/linux-debug/ntpclock_tests --gtest_filter=NtpWorker.LiveSync
```

Outbound UDP port 123 has to be open. A firewall that blocks it leaves the bottom line on the error, and the digits stay on the computer's own clock.

### Android signing and install

The Android build signs the APK. The password is an environment variable. It is not stored in the repo.

With nothing else set, signing uses the waterfall keystore: `~/.android/waterfall-release.keystore`, alias `waterfall`, and `WATERFALL_KEYSTORE_PASSWORD`. `WATERFALL_KEYSTORE`, `WATERFALL_KEY_ALIAS`, and `WATERFALL_KEY_PASSWORD` are used the same way as in the waterfall build. Set the `NTPCLOCK_*` variables only to sign with a different key.

```bash
export WATERFALL_KEYSTORE_PASSWORD='…'   # or NTPCLOCK_KEYSTORE_PASSWORD
export WATERFALL_KEY_PASSWORD='…'        # only if the key password differs
./build-android.sh
```

Create the key once, if you do not already have the waterfall one:

```bash
keytool -genkeypair -v \
  -keystore "$HOME/.android/waterfall-release.keystore" \
  -alias waterfall \
  -keyalg RSA -keysize 2048 -validity 10000
```

`keytool` will not create a keystore with an empty password.

To install onto a phone over USB, turn on USB debugging, accept the computer prompt, then:

```bash
./install-apk.sh
```

With no argument it installs the newest signed APK in `dist/`. You can also pass a path.

To publish that APK to Firebase App Distribution:

```bash
firebase login          # once per machine
./upload-firebase.sh    # newest signed APK in dist/
```

The default Firebase Android app is `1:309763775369:android:f3ca73101d839f988c9f58`. `--app` or `FIREBASE_APP_ID` selects a different one. The APK's package name has to be the package registered for that Firebase app.

With no `--groups` or `--testers`, the build is shared with the testers already on the project. `--groups` takes the group alias, not the display name. A missing alias makes Firebase return 404 after the upload, so the script checks the alias first. `--notes` or `--notes-file` sets the release notes. A CI login can use `FIREBASE_TOKEN` from `firebase login:ci`, or `GOOGLE_APPLICATION_CREDENTIALS` for a service account. The token is not stored in the repo.

`./install-prereqs.sh --mode android` installs the Firebase CLI.
