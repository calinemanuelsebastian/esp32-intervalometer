#pragma once
#include <stdint.h>
struct PanelSettings {
  uint32_t magic=0x52375031;
  // Reported cyan->yellow indicates red/blue reversal on this panel.
  uint32_t profile=1;
};
inline bool validPanelSettings(const PanelSettings& s) {
  return s.magic==0x52375031 && s.profile<=3;
}
inline const char* panelName(uint32_t p) {
  static const char* names[]={"Native","Swap R/B","Invert","Swap + invert"};
  return names[p<=3?p:1];
}
inline uint16_t panelPixel(uint16_t c,uint32_t p) {
  return p&1?uint16_t((c&0x07E0)|((c&0xF800)>>11)|((c&0x001F)<<11)):c;
}
