# ESP32 Intervalometer

A standalone photographic intervalometer with one rotary encoder, a display and isolated shutter/focus outputs. It supports timed Bulb sequences, camera-timed shooting and manual press-and-hold operation. It can be used for long exposures, time-lapse and astrophotography.

**Current firmware: v1.12 — proposed pre-release for hardware testing.** Both editions compile and pass the recorded software tests. The reported intermittent device freeze and camera EXIF timing variation have not been confirmed resolved on hardware. See [validation results](VALIDATION.txt) and [release notes](release/RELEASE-NOTES-v1.12.md).

## Choose your display

| Edition | Display | Sketch | Installation and wiring |
|---|---|---|---|
| OLED | SSD1306 128×64 monochrome I²C; optional SH1106 128×64 configuration | `OLED/R7Intervalometer/R7Intervalometer.ino` | [OLED guide](OLED/R7Intervalometer/README.md) |
| TFT | ST7735S 80×160 color SPI, used in landscape | `TFT/R7Intervalometer/R7Intervalometer.ino` | [TFT guide](TFT/R7Intervalometer/README.md) |

The two editions are complete, separate Arduino sketches. Keep each sketch's `.ino`, `.cpp` and `.h` files together. Do not combine editions. The historical `R7Intervalometer` sketch name is retained for existing installations; it does not restrict the project to one camera model.

## Features

- Timed Bulb commands from 1/10 second to 24 hours, with configurable gap, frame count and start delay.
- Camera mode with shutter-speed reminders from 1/8000 to 30 seconds. Set the speed on the camera; the remote cable cannot program it.
- Manual Shutter: press and hold to assert shutter, release to end the command, with elapsed duration shown on screen.
- Carousel navigation, adjustable encoder sensitivity and saved settings.
- Two-second hold to lock/unlock controls during a sequence, plus pause and stop controls.
- Screen sleep during shooting; OLED brightness or TFT color themes and panel calibration.
- Dry run, input/display diagnostics, GPIO command timing and Serial troubleshooting.

## Hardware and compatibility

The target is a **classic ESP32-WROOM Dev Module**. Encoder A/B/SW use GPIO32/33/27; optocoupler shutter/focus outputs use GPIO25/26. The OLED uses SDA23 and SCL18 at 3.3 V. The TFT uses the separate wiring documented in its guide.

Camera compatibility requires matching remote-contact pinout, polarity and electrical behavior. A **2.5 mm TRS socket alone does not establish compatibility**. Bulb timing requires a camera mode that keeps the exposure open while the shutter contact is held. The cable supplies no camera-state or photograph-confirmation feedback.

The fitted OLED was described as possibly SSD1306; confirm the controller and resolution before selecting its configuration. Four pins alone do not identify it. Photographs and enclosure models are not included in this release preparation.

## Install

1. Install **esp32 by Espressif Systems 3.3.12**, then select **ESP32 Dev Module** in Arduino IDE. This exact core version is required by the audited timer adapter.
2. Install **Adafruit GFX Library 1.12.6** and **Adafruit BusIO 1.17.4**. For TFT, also install **Adafruit ST7735 and ST7789 Library 1.11.0**. The OLED transport uses the core's Wire library.
3. Obtain the matching `R7Intervalometer-v1.12-OLED-MIT.zip` or `R7Intervalometer-v1.12-TFT-MIT.zip` release asset, or use the corresponding folder in this repository.
4. Replace the whole sketch folder when upgrading. Open its `.ino`; the short placeholder is intentional because Arduino compiles `App.cpp` and `PulseTimer.cpp` automatically.
5. Disconnect the camera and switch external battery power off before programming by USB. Upload, then check the **Firmware 1.12 OLED/TFT** banner at **115200 baud**.
6. Test navigation and a sequence with **Dry run ON**. Set **Dry run OFF** when ready to operate the camera.

Valid settings survive upgrading, including Dry run. Nothing starts automatically at boot. [OLED quickstart](OLED/R7Intervalometer/QUICKSTART-OLED.txt) and [TFT quickstart](TFT/R7Intervalometer/QUICKSTART-TFT.txt) give the complete setup steps.

## What changed in v1.12

Timer setup keeps shutter LOW until arming succeeds, and driver calls run outside the application's critical-section lock. A running-loop deadline check releases outputs and stops the sequence if a timer callback is missing. Display transfers yield for button/timer work instead of running menu actions or settings writes inside drawing. TFT framebuffer allocation failure no longer enters an infinite loop.

These changes address reproduced software failure paths. Timer setup can shorten a command by approximately its setup time, within a 1 ms setup budget plus instruction/clock effects. The missing-callback fallback requires a working main loop. GPIO command duration is not the camera sensor's exposure duration. See the [full technical release note](OLED/R7Intervalometer/RELEASE-1.12.txt).

## Develop and test

From the repository root, with Python 3 and a C++ compiler supporting AddressSanitizer/UndefinedBehaviorSanitizer:

```sh
python3 tools/verify_editions.py --test
```

The eight shared timer/input/sequence/capture files must remain identical between editions. The recorded v1.12 run passed four portable suites per edition, **86 OLED** and **77 TFT** integration scenarios, both ESP32 builds and the compiled normal timer-path IRAM audit.

[BUILDING.md](BUILDING.md) describes pinned dependencies, target builds and the IRAM check. The GitHub Actions workflow runs host tests and both target builds; its first remote run is pending. It has read-only repository permissions and does not publish releases.

## Troubleshooting

Serial `p` reports the switch configuration, `v` toggles input/heartbeat tracing, and `!` stops capture when the loop is responsive. OLED Serial `o` requests an idle display retry. [Startup checks](OLED/R7Intervalometer/STARTUP-CHECK.txt) cover intermittent inputs and a controlled USB-only power comparison. Reports should include firmware/display edition, power arrangement, reproduction steps and Serial output; the issue form prompts for these details.

## Project license and third-party material

The original project code and documentation are available under the [MIT License](LICENSE). Copyright (c) 2026 Emanuel-Sebastian Călin. Third-party material retains its own license; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). A copy of the MIT license is included in each downloadable sketch.
