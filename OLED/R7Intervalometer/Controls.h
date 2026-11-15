#pragma once
#include <stdint.h>

// Separate key/layout keeps v1.1-v1.3 exposure settings intact.
struct UiSettings {
  uint32_t magic=0x52375531;
  uint32_t menuClicks=2;
};
inline bool validUiSettings(const UiSettings& s) {
  return s.magic==0x52375531 && s.menuClicks>=1 && s.menuClicks<=4;
}

// Operates on decoded detents, not on raw electrical transitions.
class ScrollDivider {
public:
  void reset(){pending=0;}
  int consume(int delta,uint32_t clicks) {
    if(!delta)return 0;
    if((pending<0 && delta>0) || (pending>0 && delta<0))pending=0;
    int64_t total=(int64_t)pending+delta;
    int steps=(int)(total/clicks);
    pending=(int)(total%clicks);
    return steps;
  }
private:
  int pending=0;
};

class ManualShutter {
public:
  bool active=false, gesture=false, dryRun=true;
  uint32_t since=0,lastDuration=0;
  void press(uint32_t now,bool dry) {
    active=gesture=true;dryRun=dry;since=now;
  }
  // Release on the first observed open contact, not after release debounce.
  // Keep the gesture claimed until stable release, so bounce cannot re-trigger.
  bool releaseIfOpen(bool down,uint32_t now) {
    if(!active || down)return false;
    active=false;lastDuration=now-since;return true;
  }
  void finishGesture(){gesture=false;}
  void cancel(uint32_t now){releaseIfOpen(false,now);gesture=false;}
};
