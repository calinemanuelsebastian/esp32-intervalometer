# Changelog

## v1.12 — firmware prepared 2026-11-08

GitHub release preparation: 2026-11-15. The owner selected MIT; the licensed packages include the license without changing firmware code. Proposed status: pre-release for hardware testing; publication has not occurred as part of this preparation.

- Keep shutter LOW during timer setup, move driver calls outside the application lock and bound setup retries.
- Stop and release on a missing timer callback when the main loop remains responsive; latch a timer fault rather than count an aborted frame.
- Yield display transfers for button/timer work; defer menu actions and settings writes to the main loop.
- Preserve queued rotation without starving complete display frames.
- Replace the TFT allocation-failure infinite loop with disabled capture and live Serial diagnostics.
- Add regression scenarios and timeout each integration scenario.

No wiring or saved-settings format changes. ESP32 core 3.3.12 remains required. Physical freeze resolution and camera exposure accuracy remain unconfirmed.

[Detailed v1.12 notes](OLED/R7Intervalometer/RELEASE-1.12.txt) and [validation evidence](VALIDATION.txt).

## Earlier local releases

v1.11 improved input handling during redraws and OLED recovery. v1.10 addressed interrupt deferral during flash-cache suspension. v1.9 unified the Exposure editor and camera-speed reminders. v1.8 introduced the separate OLED and TFT editions.

Historical notes remain in each sketch folder. Use the current edition README and quickstart for wiring and operation; older notes describe earlier behavior. This history does not imply that earlier versions were published to GitHub.
