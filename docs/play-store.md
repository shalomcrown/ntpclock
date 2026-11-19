# Google Play

The signed APK from `./build-android.sh` installs over USB. Play will not accept that package as it is built today. Two platform requirements are still open, and a few listing items have to be pasted into Play Console by hand.

Package name: `com.kirayim.ntpclock`. Version name `0.1.0`, version code `1`.

## What blocks an upload

Play requires a new app to target Android 16 (API 36). This build targets API 33, because Qt 6.6.3 and `scripts/prereqs.env` pin `ANDROID_PLATFORM=android-33`, and the only platform installed under `/opt/android-sdk/platforms` is `android-33`. The requirement took effect on 31 August 2026. The extension date was 1 November 2026.

Play also requires native libraries to be aligned to 16 KB pages. `libntpclock` and `libQt6Core` in the current APK are aligned to 4 KB (`0x1000`). Relinking only this app does not fix Qt's prebuilt libraries. A newer Qt Android kit, built with a current NDK and `-Wl,-z,max-page-size=16384`, is the way through both of these.

Play Console wants an Android App Bundle, not an APK. After the kit can target API 36, `bundleRelease` in the generated `android-build` project produces the `.aab`. Sign it with the same release keystore the APK already uses, and enroll that key in Play App Signing. Losing the keystore means you cannot ship updates.

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
