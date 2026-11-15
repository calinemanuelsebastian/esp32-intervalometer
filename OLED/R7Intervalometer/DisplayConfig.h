#pragma once
#include <stdint.h>

// Confirmed wiring reuses the old LCD's SCL/SDA pins, not ESP32 I2C defaults.
constexpr int OLED_SDA=23, OLED_SCL=18;
constexpr uint32_t OLED_I2C_HZ=400000; // try 100000 if long wiring is unreliable
constexpr uint16_t OLED_TIMEOUT_MS=10;
constexpr int OLED_WIDTH=128, OLED_HEIGHT=64;
constexpr uint8_t OLED_ADDRESS=0; // 0 probes 0x3C then 0x3D; or force either address
// Controller cannot be identified from its I2C address. SSD1306 is the working
// assumption; set true only if the module is an SH1106 128x64 instead.
constexpr bool OLED_SH1106=false;
static_assert(OLED_ADDRESS==0 || OLED_ADDRESS==0x3C || OLED_ADDRESS==0x3D,"OLED address must be 0, 0x3C or 0x3D");
static_assert(OLED_WIDTH==128 && OLED_HEIGHT==64,"This UI requires a 128x64 screen");

struct OledSettings {uint32_t magic=0x52374F31,contrast=96;};
inline bool validOledSettings(const OledSettings& s) {
  return s.magic==0x52374F31 && s.contrast<=255;
}
