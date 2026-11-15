#pragma once
#include <stdint.h>

// Separate NVS record preserves every earlier exposure/menu setting.
constexpr uint32_t THEME_COUNT=4;
struct ThemeSettings {uint32_t magic=0x52375431,profile=0;};
inline bool validThemeSettings(const ThemeSettings& s) {
  return s.magic==0x52375431 && s.profile<THEME_COUNT;
}
struct Palette {uint16_t bg,panel,fg,muted,accent,warning;};
inline const char* themeName(uint32_t i) {
  static const char* names[]={"Cyan","Night red","Amber","Green"};
  return names[i<THEME_COUNT?i:0];
}
inline Palette themePalette(uint32_t i) {
  static const Palette palettes[]={
    {0x0841,0x18E3,0xFFFF,0x9CF3,0x07FF,0xFFE0},
    {0x0000,0x2800,0xB800,0x7800,0xF800,0xD800},
    {0x0000,0x30A0,0xFD20,0xA2A0,0xFE00,0xFFE0},
    {0x0000,0x0140,0x07E0,0x03C0,0xBFF7,0xCFE0}
  };
  return palettes[i<THEME_COUNT?i:0];
}
