# Google Play

`./build-android.sh` writes two signed files. `dist/ntpclock-<version>-arm64-v8a.apk` installs over USB. `dist/ntpclock-<version>-arm64-v8a.aab` is the file Play Console accepts. A few listing items still have to be pasted into Play Console by hand.

Package name: `com.kirayim.ntpclock`. Version name `0.1.0`, version code `1`. Minimum Android version: 9 (API 28). Target: Android 16 (API 36).

## What the package already satisfies

The Android kit is Qt 6.11.3, with NDK 27.2.12479018, SDK platform android-36, build-tools 36.0.0, and JDK 21. Those pins live in `scripts/prereqs.env`. That NDK's CMake toolchain only accepts a native platform through android-35, so the C++ sysroot is android-35 while Gradle still compiles and targets API 36. Qt 6.11 supports Android 9 through 16, and its libraries are built for 16 KB pages. The app link uses `-Wl,-z,max-page-size=16384`. `build-android.sh` refuses to finish unless the APK and the bundle both have LOAD alignment of at least `0x4000`, and unless the APK's target SDK is 36 and its minimum SDK is 28.

Play App Signing should enroll the same release keystore the script already uses. Losing that keystore means you cannot ship updates. The password stays in the environment. It is not in the repo.

## Listing text

Short description (80 characters):

> A large NTP clock for measuring the delay of a filmed screen.

Full description:

> NTP Clock shows the time in digits large enough to film. Put it on one screen, film it with another, and read the milliseconds.
>
> The app can also photograph the other screen. It reads the timing block beside the milliseconds and shows how far behind that screen is. The photograph stays on the device for that comparison and is not saved or uploaded.
>
> The clock uses public NTP servers when you allow network access. If you refuse, it shows the device clock.
>
> Source and license: https://github.com/shalomcrown/ntpclock

## Privacy and data safety

Privacy policy URL, once this file is on the default branch:

https://github.com/shalomcrown/ntpclock/blob/main/PRIVACY.md

Play sometimes wants a normal web page rather than a GitHub file view. If the console rejects that URL, publish `PRIVACY.md` as a page and use that address.

Data safety answers:

- Camera: collected, not shared, processed in memory only, purpose is app functionality. The picture is not stored.
- Device or other IDs: not collected.
- Location, contacts, photos library, financial info: not collected.
- NTP: the app sends a time query. No personal data is in it.
- No ads, no account, no analytics, no crash SDK.

## Artwork already in the repo

- Phone screenshots: `docs/images/clock.jpg`, `docs/images/portrait.jpg`, `docs/images/compare.jpg`
- 512×512 icon: `docs/play-icon-512.png`

Still to make in the console: a 1024×500 feature graphic, and a content rating questionnaire. The app has no violence, no user chat, and no unrestricted web. Category: Tools.
