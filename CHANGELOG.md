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

### Fixed

- Android: the bottom (portrait) or right side (landscape) no longer runs off the screen below the camera cutout.
