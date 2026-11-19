# Changelog

All notable changes are documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/). Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Full-screen NTP clock for measuring video latency. The time and milliseconds are drawn as large as the window allows.
- Android signing uses the waterfall keystore when an ntpclock keystore is not set.
- Background NTP sync at startup, every five minutes, and after wake from suspend or sleep.
- Linux DEB, Windows NSIS, and Android APK build scripts.
- Camera inset that photographs another screen and shows it beside a screenshot of this clock.
- Version line at the top of the clock: `<version>.<UTC build time YYYYMMDDHHmm>.<git hash>`.
- Gray-code timing block beside the milliseconds, and the compare screen reads it to show how far behind the other screen is.

### Fixed

- Android: the bottom (portrait) or right side (landscape) no longer runs off the screen below the camera cutout.
- The phone clock was updating only a few times a second because each camera frame was converted on the UI thread. The phone now asks for a 120 Hz refresh, and a soft photo of the timing block can still be read.
