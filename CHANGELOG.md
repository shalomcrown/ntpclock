# Changelog

All notable changes are documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/). Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Android App Bundle beside the APK. Play Console takes the bundle. A phone install over USB still uses the APK.
- Full-screen NTP clock for measuring video latency. The time and milliseconds are drawn as large as the window allows.
- Guide page that shows this readme, with the running version on the first line. Open it with Guide on the clock.
- Android signing uses the waterfall keystore when an ntpclock keystore is not set.
- Background NTP sync at startup, every five minutes, and after wake from suspend or sleep.
- Linux DEB, Windows NSIS, and Android APK build scripts.
- Camera inset that photographs another screen and shows it beside a screenshot of this clock.
- Version line at the top of the clock: `<version>.<UTC build time YYYYMMDDHHmm>.<git hash>`.
- Gray-code timing block beside the milliseconds, and the compare screen reads it to show how far behind the other screen is.

### Changed

- Android builds use Qt 6.11.3 and target Android 16 (API 36). Native libraries are aligned to 16 KB pages. The phone needs Android 9 or newer.

### Fixed

- A tiny bright patch could be taken as the timing block, and the compare line showed only the milliseconds, so a 163 ms gap was reported as 1219 ms ahead. Patches that are small beside the picture are ignored. When the other screen's HH:mm:ss and milliseconds are clear they are read by shape, without assuming the two devices share a font, and both times are shown.
- Dragging a finger on the guide was selecting the text. The drag now scrolls the page, and links still open.
- Android: the bottom (portrait) or right side (landscape) no longer runs off the screen below the camera cutout.
- The phone clock was updating only a few times a second because each camera frame was converted on the UI thread. The phone now asks for a 120 Hz refresh, and a soft photo of the timing block can still be read.
- Portrait compare: the latency line is kept when the shot is stored, before that page is on screen.
- A slanted photo of the timing block was read as every cell white, so a 300 ms gap was reported as 1434 ms. The reader now follows a partial slanted edge and samples the frame bar rather than the middle of the border cell.
- A close-up camera frame of the same block was still read as every cell white, so the difference came out about 1400 ms. The reader now searches that bright frame one pixel at a time and keeps the reading when one side is cut off by the picture.
- The seconds line could sit still for a few seconds. A milliseconds repaint was counted as a drawn second, so the hours and seconds were not painted again until much later.
