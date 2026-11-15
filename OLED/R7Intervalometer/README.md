# ESP32 photographic intervalometer — firmware 1.12 OLED

ESP32-WROOM Dev Module, **128×64 monochrome I²C OLED**, five-pin mechanical encoder with push switch, and two optocouplers. This OLED edition has a native 128×64 layout. Version 1.12 separates timer setup and menu actions from critical sections and display transfers, adds a missing-callback fault response, and retains OLED communication recovery. The single Exposure editor, fractional shutter-speed labels and Camera-mode speed reminder from v1.9 are retained. Timed Bulb sequences, camera-timed triggering, manual hold-to-shoot, adjustable scrolling, control lock and timing diagnostics are retained.

**SSD1306 128×64 is a working assumption, not a confirmed identification of the fitted display.** Four pins alone do not identify its controller or resolution. SH1106 128×64 is an explicit alternative in `DisplayConfig.h`; 128×32 and other controllers are not supported by this layout. Start with `QUICKSTART-OLED.txt`.

The device is intended for general photographic use; astrophotography is the builder's main application. The builder has tested it with a Canon EOS R7. Other cameras require a compatible wired-remote interface: the **2.5 mm TRS connector alone does not establish compatibility**. Check shutter/focus/common pin assignments, electrical levels and polarity, and contact-closure operation against the output circuit below. Timed sequences require a camera mode in which holding the remote shutter contact keeps the exposure open. The historical sketch/folder name `R7Intervalometer` is retained for existing installations.

## Install

1. In Arduino IDE, install **esp32 by Espressif Systems 3.3.12**. Select **ESP32 Dev Module**.
2. Install **Adafruit GFX Library** and its **Adafruit BusIO** dependency. Wire is included with the ESP32 core. No separate SSD1306, SH110X or ST7735 library is required; `OledDisplay.h` contains the checked I²C transport.
3. Open `R7Intervalometer.ino`. Keep **all supplied .cpp and .h files**, including **DisplayConfig.h, OledDisplay.h, Exposure.h, TimerPlatform.h and PulseTimer.cpp**, alongside it. The .ino is intentionally a placeholder; Arduino builds App.cpp automatically.
4. Disconnect the camera and switch external battery power off before programming through ESP32 USB. Avoid powering an unverified clone board from external 5 V and USB simultaneously.
5. Upload. Serial Monitor at 115200 baud identifies **Firmware 1.12 OLED** and reports the configured controller, I²C address and initialization result.

Existing valid exposure, capture and encoder sensitivity settings are retained. This includes **Dry run**, so check its value after upgrading. New/invalid settings default to Dry run ON and two encoder detents per menu item. OLED brightness has its own saved record, defaulting to 96/255; old Theme and Panel records are ignored. No exposure starts automatically at boot. The control lock is temporary and is not saved. Replace the entire sketch folder when upgrading; do not copy App.cpp alone.

## Startup/input diagnosis without a menu

Home shows **SW:1** when GPIO27 is HIGH (normally released) and **SW:0** when LOW. **Release knob** means the boot/recovery guard still awaits a stable release. A frozen display can show a stale value. Both editions check for button/timer events between display transfers and yield the frame to the main loop. Menu actions, flash saves and Serial logs no longer run inside the renderer. Rotation stays queued until the main loop, where it is handled before a confirming click; continuous rotation does not starve display completion.

Serial at **115200** prints the reset reason and button state at boot. Send `p` for GPIO/debouncer state or `v` for continuous input/heartbeat tracing without entering a menu. `BTN down` is a pressed boolean, whereas `SW`/`RAW` are GPIO levels. A brownout reset is identified explicitly. A missing brownout report does not establish a stable supply. See `STARTUP-CHECK.txt` for the USB-only comparison and voltage checks.

After an OLED I²C failure, capture stops and the firmware retries no more often than once a second while the button is stably released. Recovery returns to Home and requires release before a fresh press. It never resumes an interrupted exposure. Persistent display faults continue to block capture; Serial `o` remains available for a manual retry.

## Wiring

Numbers are GPIO labels, not physical header positions. The new display reuses GPIO18 and GPIO23, now configured as **I²C**, not SPI. These are deliberately different from the usual ESP32 I²C defaults.

| Device contact | ESP32 / destination |
|---|---|
| OLED GND | GND |
| OLED VIN | **3V3** (confirmed supply) |
| OLED SCL | **GPIO18** (I²C clock) |
| OLED SDA | **GPIO23** (I²C data) |
| Encoder A / B / common | GPIO32 / GPIO33 / GND |
| Encoder push contacts | GPIO27 and GND |
| Shutter optocoupler LED anode / cathode | GPIO25 through 220–330 Ω / GND |
| Focus optocoupler LED anode / cathode | GPIO26 through 220–330 Ω / GND |
| Each GPIO25 / GPIO26 | 10 kΩ pull-down to GND |
| Shutter optocoupler collector / emitter | Camera 2.5 mm TRS tip / sleeve |
| Focus optocoupler collector / emitter | Camera 2.5 mm TRS ring / sleeve |

For PC817/EL817 DIP packages, verify orientation: LED anode/cathode are pins 1/2 and emitter/collector are pins 3/4. Camera sleeve stays isolated from ESP32 ground. All encoder inputs use internal pull-ups; optional external 10 kΩ pull-ups go to **3V3**, never VIN.

The old display's **RES/GPIO19, DC/GPIO22, CS/GPIO21 and BLK/GPIO13 connections are no longer used**. Leave those old display wires disconnected. The new display needs only its four connections. Firmware does not drive the old display pins. OLED supply stays connected; screen blanking uses controller commands.

I²C SDA/SCL require pull-ups to 3.3 V; many modules already include them. If this module has none, fit approximately 4.7 kΩ from each signal to 3.3 V. Never pull the ESP32 signals up to 5 V. Pinout, module revision and optocoupler behavior still require verification on the assembled device.

## Everyday controls

Press on Home to enter the carousel: **Start → Manual Shutter → Settings → Diagnostics → Back**.

- Rotate to select; press/release to open or confirm.
- Menu movement defaults to two detents per item. **Settings → Menu clicks** chooses 1–4 and saves it. This affects carousel/menu selection, including the Manual Shutter Back selection.
- While editing Exposure, each decoded detent selects the next offered duration. Other fields change by one unit per detent. There is no fast-turn acceleration.
- Press to save an edited value and return; hold two seconds to cancel that edit.
- Rotation on Home wakes the screen without navigating.

## OLED display and brightness

**Settings → Brightness** previews a controller contrast value from 0 to 255. Press to save; hold two seconds to cancel. Zero is minimum contrast, not screen OFF. Physical pixel color is fixed by the OLED panel, including any fixed yellow/blue zones. RGB themes and LCD color calibration are therefore removed.

The driver probes **0x3C, then 0x3D**. An ACK confirms an address responds; it does not identify the controller or prove that visible pixels work. **Diagnostics → OLED info** reports the configured controller and detected address.

`DisplayConfig.h` controls SDA/SCL, bus speed, address and controller. Default `OLED_SH1106=false` selects SSD1306. Set it to `true` and recompile only for an SH1106 128×64 module. `OLED_ADDRESS=0` probes both addresses; it can instead be forced to 0x3C or 0x3D. Default speed is 400 kHz; 100 kHz is an option when investigating wiring problems.

Every I²C transaction checks its result and uses a 10 ms Wire timeout. After a failed transaction, the firmware marks the screen offline, stops any sequence/manual hold and releases the camera outputs. New captures are blocked while offline. Automatic retries are spaced by at least one second, with outputs released and the button stably open. Serial **`o`** also retries initialization while idle and returns Home without restarting capture. A disconnected screen is detected when a transfer is attempted, not immediately while sleeping. This handles reported bus errors; it cannot detect a wrong controller that acknowledges commands or a physically blank but responsive panel.

## Manual Shutter

Select **Manual Shutter**, then leave **Hold to shoot** highlighted.

- The press used to enter this page does not take a photo.
- Selecting Hold to shoot holds the focus/half-press contact to keep the camera ready (LIVE only). Selecting Back or leaving the page releases it. This may start autofocus if the camera is configured to focus on half-press.
- The next press, held stable for **5 ms**, asserts shutter. Menu presses still use 30 ms. Release ends shutter on the first observed open contact, without waiting another 30 ms for release debounce; the focus contact stays held while Hold to shoot remains selected.
- Display transfers use 16-byte data packets and check for pending manual input between completed I²C transactions, yielding to the main loop for GPIO actions. A full frame no longer blocks manual input for its entire transfer. Camera wake-up, autofocus, self-timer and shutter lag can still add delay: use manual focus and single-shot drive when assessing the electronic response.
- In ordinary still-photo modes, a tap acts like a wired remote press. Camera exposure/drive/AF settings determine what it records; use single-shot drive for one photo per press.
- For hold-to-expose, use a compatible **B/Bulb** mode, disable any internal Bulb timer or toggle-to-expose option, and use manual focus. Keep the encoder button held for the whole exposure. There is no toggle/latch mode and no automatic 500 ms pre-focus delay in Manual Shutter.
- Holding longer than two seconds does **not** invoke Back or Lock/Unlock in this mode.
- Rotation during a press can wake the display but cannot change the selection. Release first, then rotate to **Back** and press to leave.
- Last duration shows the commanded hold time, not measured camera exposure time. Dry run ON simulates the same behavior while keeping camera outputs LOW.

The OLED switches off when a manual trigger starts, wakes on rotation, times out after Wake time while held, and turns on at release. Short contact bounce on release cannot start a second exposure. A detected open contact ends the hold; a full firmware stall cannot service that release until execution resumes.

The camera cannot report its mode, focus state or saved files through this cable. Bulb behavior follows the camera's configuration: [Canon R7 Bulb guide](https://cam.start.canon/en/C005/manual/html/UG-03_CustomShooting_0070.html).

## Timed sequences

| Setting | Range | Default |
|---|---|---|
| Exposure in Bulb mode | Fractional choices from 1/10 s; then 1–86,400 s | 90 s |
| Exposure in Camera mode | Camera-speed reminder, 1/8000–30 s | 1/100 s |
| Shot mode | Bulb / Camera | Bulb |
| Trigger pulse | 50–1,000 ms, Camera mode only | 100 ms |
| Gap, shutter closed | 1–3,600 seconds | 3 |
| Frames | 1–9,999; 0 = unlimited | 20 |
| Start delay | 0–3,600 seconds | 5 |
| Wake time | 1–120 seconds | 10 |
| Dry run | ON / OFF | ON |
| Menu clicks | 1–4 detents per item | 2 |
| Brightness | 0–255 controller contrast | 96 |

**Exposure** replaces the separate Bulb sec and Bulb ms fields. Press to edit, rotate to select, press to save, or hold two seconds to cancel. The value and its behavior depend on **Shot mode**.

**Bulb mode:** available subsecond choices are **1/10, 1/8, 1/5, 1/4 and 1/2 s**, followed by **1–86,400 s** in one-second steps. There is no zero-second choice. These fractions are exactly representable by the existing millisecond command timer; the minimum remains 1/10 s. This is the electrical hold duration, not a guarantee that a particular camera accepts that Bulb time. The camera needs hold-to-expose Bulb operation.

Existing custom durations are preserved exactly. A saved 10.375 s still displays and commands 10.375 s; it is not silently rounded on upgrade or when opening/saving without rotation. Rotating moves to the next offered duration in that direction. Custom values that are not exact reciprocals appear in decimal seconds. For example, 0.333 s is not mislabeled as 1/3 s.

**Camera mode:** Exposure is a **reminder to set the same speed on the camera**. It includes fractional choices such as **1/100, 1/60 and 1/30 s**, ranging from 1/8000 to 1/2 s, followed by 1–30 s. Choose only a speed the camera supports. The editor says **Reminder: set camera**, and Home says **Set camera: 1/60 s**, for example. This value is saved independently of the Bulb duration.

The reminder does **not** set the camera's speed, change the remote pulse, adjust Gap or confirm the camera's settings. The intervalometer sends a normal **Trigger pulse** (default 100 ms), with the usual focus lead. That pulse duration is an electrical control setting and remains labeled in milliseconds. Set the camera's shutter speed yourself in M/Tv or its equivalent. The screen says TRIGGERING and counts triggers, not confirmed photographs. Use single-shot drive and set Gap long enough for the camera exposure and saving to finish; Gap starts when the remote press ends and there is no busy feedback. Camera mode can also let the camera time a 10-second exposure internally instead of using external Bulb control.

Set **Dry run OFF** to operate the camera. It applies to both timed sequences and Manual Shutter and persists across power cycles.

Start uses a snapshot of the exposure settings. Focus is requested at least 500 ms before shutter. A zero start delay includes that wake time. If a loop is late, the gap extends to preserve the focus lead; missed shots are not replayed in a burst. The gap starts at the timer interrupt's release timestamp, not when the display loop later notices completion. It has millisecond scheduling resolution and may lengthen when the loop is busy.

During an unlocked sequence, press for controls: Return, Pause after frame / Resume, Stop now. Pausing during exposure waits for the frame to finish; pausing during delay/gap is immediate. Resume waits a full gap. An interrupted exposure is not counted as a completed frame.

**Hold approximately two seconds to toggle Lock/Unlock**, from the running screen or its controls menu. The screen shows LOCKED or UNLOCKED. Each new sequence starts unlocked. While locked, rotation and a short press only wake the display; they cannot select a menu item, pause or stop the sequence. Holding the button continuously toggles once, and releasing it does not generate a second action. To stop, unlock, press to open the controls and select **Stop now**. This replaces the old long-hold-to-stop shortcut.

The lock remains active through exposures, gaps, the initial delay and a paused sequence. It clears on completion, stop, timer failure or reboot. A button gesture begun during a sequence is consumed if the sequence ends before that gesture finishes, preventing accidental Home/Back navigation. Locking never changes the current exposure deadline and never latches a manually held shutter.

The OLED goes off at each exposure start, wakes on interaction, times out while exposing, and lights during gaps, pause and completion.

## Exposure timing and EXIF

Timed commands use a one-shot **1 MHz ESP32 GPTimer** with a 64-bit deadline. Its interrupt releases shutter first and focus second, independently of display updates. **Version 1.10 fixes the interrupt's flash-cache handling in both display editions.** The older ISR was placed in IRAM, but the stock driver's interrupt allocation could still defer it while the flash cache was disabled.

`TimerPlatform.h` now marks the dedicated interrupt as IRAM-safe before enabling it. The callback uses direct GPIO registers and an IRAM-resident clock; it no longer calls the flash-resident `gptimer_get_raw_count()` or `millis()`. Its context and driver object must be in internal RAM. The compiled interrupt call chain is checked, not only the application's callback. This adapter uses one private IDF accessor, so **ESP32 core 3.3.12 / IDF 5.5.5 and classic ESP32 are required**; other builds are deliberately rejected pending an audit. See `RELEASE-1.10.txt`.

Alarm setup now keeps shutter LOW while driver calls run outside the application lock. The final IRAM assertion accepts at most 1 ms of setup time, with up to three bounded attempts after setup preemption. Consequently the command can be shorter than requested by approximately the setup time (at most 1 ms), plus instruction/clock effects; diagnostics report the resulting GPIO interval. Driver failure, an expired deadline or exhausted retries aborts with outputs LOW.

If a callback is missing, the running main loop releases the outputs once it observes the command at least 20 ms overdue, stops the sequence without counting that frame, and latches **TIMER ERROR: REBOOT**. This is a fault response requiring a working loop, not a guaranteed maximum latency or an alternative precision timer. A total processor, driver or power lockup can prevent it. See `RELEASE-1.12.txt`.

Stale callbacks cannot terminate a new pulse or manual hold. A completed pulse cannot be reasserted while the main loop catches up. Dry run uses the same alarm path with GPIOs kept LOW. Timer or interrupt-configuration failure blocks capture until reboot. Settings writes remain excluded during active sequences and manual gestures.

This is still an interrupt-driven release. The counter's 1 µs resolution is **not a promise of 1 µs absolute accuracy**. Global interrupt masking, higher-priority work, clock tolerance and optocoupler/camera response still matter. Manual Shutter is a button hold whose release is serviced by the main loop. No guessed timing offset is applied.

**Diagnostics → Timing** shows the most recent timed pulse:

- **Set:** requested trigger duration.
- **Got:** approximate duration from IRAM clock readings immediately before GPIO assertion and after release, shown to milliseconds.
- **Error:** measured minus requested, in microseconds, for a completed pulse.
- **LIVE / GPIO timestamps** or **DRY / simulated output** distinguishes physical GPIO commands from a dry run. A cancelled pulse is marked **Stopped early**; a missing-callback fault is marked **Release timeout**.

**Diagnostics → Timing series** shows measured completed pulses, missing timestamp readings, minimum, maximum and spread in microseconds. It resets at each new sequence. Serial reports each pulse and aggregate statistics, including maximum IRQ-entry lateness. The ISR itself never logs or writes flash. An injected-latency test verifies that this report exposes a 100 ms timing error rather than hiding it. DRY reports are simulated commands, never camera measurements.

The readings use the ESP32's own clock; they cannot independently verify clock accuracy, the camera connector or the sensor's exposure. Serial Monitor at 115200 also reports each completed pulse when UART space is available. This logging is best effort so it does not wait for the serial port.

For the reported 10.0 / 10.1 / 10.2 s EXIF values, run 20 frames at 10 s with a suitable gap, camera **B**, internal Bulb timer **disabled**, manual focus and single-shot drive. Compare the trigger logs with the photos. If the trigger durations remain steady but EXIF varies, firmware loop delay is not the explanation for that remaining variation; check the electrical output with a scope/logic analyzer and the camera's behavior. No guessed 100–200 ms correction is applied. This two-contact remote connection provides no camera feedback.

Implementation references: [Espressif GPTimer](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/peripherals/gptimer.html), [Canon R7 Bulb operation](https://cam.start.canon/en/C005/manual/html/UG-03_CustomShooting_0070.html).

## Diagnostics

The Diagnostics menu includes Encoder, Display, Screen sleep, System info, Timing, Timing series and OLED info. Encoder test shows raw A/B/SW, decoded steps before menu division and click/press counts. Display test has monochrome stripes, a border, a moving marker and frame counter. Screen sleep test alternates on/off every second for eight seconds.

Runtime input logging is disabled by default for normal use (`INPUT_DEBUG=false`); the on-device diagnostics remain available and Serial `v` toggles runtime input tracing. Serial commands at **115200 baud**:

| Command | Action |
|---|---|
| `d` / `e` / `t` / `b` / `i` | Diagnostics menu / encoder / display / screen sleep / system |
| `r` | Last timed trigger duration / error |
| `s` | Per-sequence timing statistics |
| `c` | OLED configuration and address |
| `o` | Retry OLED initialization while idle; return Home |
| `v` | Toggle nonblocking input/heartbeat logging, including lock state |
| `!` | Stop, release both outputs, clear lock, reinitialize inputs and return Home |
| `q` | Home |
| `p` | Read GPIO27 input/output and pull-up/down configuration |
| `u` | Report GPIO27, reapply INPUT_PULLUP, reset the debounce state, report again |
| `?` | Help |

Page-changing diagnostics and `u` reject active/paused sequences and manual button gestures. Read-only `p`, trace `v`, help `?` and explicit recovery `!` remain available. Recovery never restarts an exposure and requires a held button to be released before accepting another press. It needs a running main loop; it cannot recover a completely stalled CPU or a power fault. Expected released GPIO27 configuration: `IN=1 OUT=0 PU=1 PD=0 RAW=1`. Config readback does not measure voltage. The `u` command requires a held button to be released before a subsequent click can act.

Configuration constants at the top of App.cpp:

- `ENCODER_EDGES_PER_STEP=2`: electrical calibration for the 15-pulse/30-detent encoder. Use 4 only if one physical detent produces two decoded steps. **Menu clicks** is a separate UI setting.
- `ENCODER_DIRECTION=1`: use -1 to reverse direction.

Display configuration is separate in `DisplayConfig.h`, described above.

## Power and enclosure

Read `POWER-AND-ENCLOSURE.txt` before locating switch/USB openings. The normal switch should cut the converter's input supply after battery protection, so converter, ESP32 and OLED all lose power. A battery master disconnect has different charging behavior. The exact board wiring and the cause of the reported recovery problem are not confirmed.

## Validation and device check

Compilation and host tests are described in `RELEASE-1.12.txt`. Run `python3 tests/run_host_tests.py` to reproduce the host suites. The source builds with ESP32 core 3.3.12, GFX 1.12.6 and BusIO 1.17.4. Earlier release notes, `GHID-1.7-RO.txt` and `AUDIT.txt` are historical; their display wiring and controls may differ. Use this README and `QUICKSTART-OLED.txt` for the current wiring and interface.

Before using a long real exposure:

1. With the camera disconnected, test menu sensitivity and a three-frame Dry run. Hold two seconds to lock, check that a short press/rotation only wakes the display, then unlock and use Stop now. Repeat through normal completion and confirm that the lock clears.
2. In Manual Shutter with Dry run ON, verify a short press and a 5-second hold. The hold must stay in Manual Shutter, end on release and show the duration. Rotate to Back and exit.
3. Check the camera-side optocouplers using an appropriate meter test with the camera disconnected, then test one short live exposure with Dry run OFF.
4. Verify power OFF actually extinguishes both ESP32 and OLED with USB disconnected, and that ON boots at Home with shutter released.

These are bench checks for the assembled device; no device has been flashed or tested by this workspace. Timed exposure release is independent of UI transfers, but interrupt masking or a total firmware/power fault can still delay or prevent it. Manual release remains dependent on the loop and its button polling between I²C transactions. Physical timing accuracy and the reported EXIF variation have not been verified on the assembled device. The physical power switch and output pull-downs remain necessary.
