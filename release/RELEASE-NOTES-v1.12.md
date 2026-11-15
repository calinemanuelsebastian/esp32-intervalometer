**ESP32 Intervalometer v1.12 — timer and display stability**

This pre-release provides separate OLED and TFT firmware editions for hardware testing. It fixes reproduced software failure paths; the intermittent freeze and EXIF exposure variation reported on the assembled device have not yet been confirmed resolved.

### Fixes

- Shutter remains LOW while its timer is armed. Driver calls run outside the application's critical-section lock, with bounded setup retries.
- If a timer callback is missing, the responsive main loop releases outputs, stops the sequence and reports a timer fault. The interrupted frame is not counted; reboot is required before retrying capture.
- Display transfers yield for pending button/timer work. Menu actions and settings saves run in the main loop, and rotation cannot starve every complete frame.
- TFT framebuffer allocation failure no longer traps startup in an infinite loop. Capture is blocked and Serial diagnostics remain available.

### Downloads

| Asset | Use |
|---|---|
| `R7Intervalometer-v1.12-OLED-MIT.zip` | Complete Arduino source sketch for SSD1306 128×64 I²C OLED; configurable SH1106 alternative |
| `R7Intervalometer-v1.12-TFT-MIT.zip` | Complete Arduino source sketch for ST7735S 80×160 SPI TFT |
| `R7Intervalometer-v1.12-Launch-MIT.zip` | Both sketches, detailed guides, validation report and verification tools |
| `R7Intervalometer-v1.12-MIT-SHA256SUMS.txt` | SHA-256 checksums for the three ZIP assets |

These are source packages for Arduino IDE, not precompiled flash images. The automatically generated GitHub source archives contain the repository layout with both editions.

### Upgrade

Use **esp32 by Espressif Systems 3.3.12**, board **ESP32 Dev Module**. Replace the entire sketch folder, including all `.cpp` and `.h` files. Do not mix editions. OLED requires GFX 1.12.6 and BusIO 1.17.4; TFT additionally requires ST7735/ST7789 1.11.0. No wiring changes are required from the corresponding v1.11 edition.

Existing valid settings, including Dry run, are retained. Test with Dry run ON before operating the camera. Serial at 115200 baud identifies **Firmware 1.12 OLED** or **Firmware 1.12 TFT**. Nothing starts automatically at boot.

### Validation and remaining limits

Both ESP32 targets compiled with warnings enabled. Four portable test suites per edition, 86 OLED and 77 TFT integration scenarios passed, as did the compiled normal interrupt-path checks. Host integration tests use address and undefined-behavior sanitizers.

No hardware flashing, supply measurements or camera timing measurements were performed for this release validation. A total CPU, driver or power lockup can prevent the foreground fallback from running. GPIO command timestamps do not establish sensor exposure accuracy. The fitted OLED controller still needs positive identification, and a 2.5 mm connector alone does not establish camera compatibility.

Photos and enclosure models are not included.

### License

Original project code and documentation are licensed under MIT. Each sketch ZIP includes `LICENSE`; the bundled Adafruit font and external dependencies retain their own licenses. The `MIT` filename suffix identifies the licensed packaging revision, with firmware code unchanged from v1.12.
