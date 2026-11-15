# Building and validation

The supported target is `esp32:esp32:esp32` (classic ESP32 Dev Module), **ESP32 Arduino core 3.3.12 / ESP-IDF 5.5.5**. `TimerPlatform.h` deliberately rejects other core versions pending another interrupt-path audit. It uses a private IDF accessor, so updating the core is not a routine dependency bump.

Pinned display dependencies:

| Library | Version | Edition |
|---|---|---|
| Adafruit BusIO | 1.17.4 | Both |
| Adafruit GFX Library | 1.12.6 | Both |
| Adafruit ST7735 and ST7789 Library | 1.11.0 | TFT |

The workflow installs those libraries with `--no-deps`; unused example dependencies from their library metadata are not needed by these sketches. Wire, SPI and Preferences come from the ESP32 core.

## Host tests

Use Python 3 and a C++ compiler with address/undefined-behavior sanitizers. Run from the repository root:

```sh
python3 tools/verify_editions.py --test
```

The runner uses temporary binaries and gives each application scenario a 15-second timeout. Set `CXX=clang++` if needed. The OLED and TFT suites use host substitutes for peripherals; they do not establish physical camera or power behavior.

## Arduino CLI

Follow the installation commands in [.github/workflows/ci.yml](.github/workflows/ci.yml), or install the same dependencies in Arduino IDE. The workflow pins Arduino CLI 1.3.1. Recorded local builds used CLI 1.5.2-rc.1 with the same core and display library versions; the first GitHub-hosted run is pending.

To compile directly after dependency installation:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --build-path build/OLED OLED/R7Intervalometer
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --build-path build/TFT TFT/R7Intervalometer
```

The convenience script compiles and immediately runs the timer-path audit. Supply the `xtensa-esp32-elf-objdump` executable installed by the pinned core. Its relative path under the Arduino data directory is `packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-objdump` (add `.exe` on Windows). With the workflow's local `.arduino/data` directory:

```sh
python3 tools/build_firmware.py OLED --objdump .arduino/data/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-objdump
python3 tools/build_firmware.py TFT --objdump .arduino/data/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32-elf-objdump
```

Use `--config-file` for a custom Arduino configuration, `--arduino-cli` for a custom CLI path, and `--build-root` for an alternate output folder. This script never uploads firmware.

The IRAM audit checks normal direct-call paths and the pinned indirect call sites, including final shutter assertion, GPIO release and callback context placement. It does not prove worst-case execution time or correct behavior during a total processor or power failure. [VALIDATION.txt](VALIDATION.txt) records the v1.12 results.

## Release assets

The three MIT-packaged v1.12 ZIP assets are frozen. These contain the unchanged firmware plus licensing and package-documentation additions. Earlier v1.12 ZIPs are preserved separately with their original hashes. With them and their checksum file in a sibling `release-assets` directory:

```sh
python3 tools/verify_release_assets.py ../release-assets
```

This checks SHA-256, ZIP CRCs and every payload file against the repository. Intentional later firmware changes require a new version and new asset hashes; do not replace a published version silently.

The workflow performs tests/builds only. It has read-only repository permissions, runs ordinary pull-request events, and contains no release-publishing job. The action references are pinned to the upstream [checkout v7.0.0 commit](https://github.com/actions/checkout/commit/9c091bb21b7c1c1d1991bb908d89e4e9dddfe3e0) and [setup-arduino-cli v2.0.0 commit](https://github.com/arduino/setup-arduino-cli/commit/81d310742121c928ea9c8bbd407b4217b432ae02). Arduino documents [versioned core installation](https://arduino.github.io/arduino-cli/1.2/commands/arduino-cli_core_install/) and [CLI setup in Actions](https://github.com/arduino/setup-arduino-cli).
