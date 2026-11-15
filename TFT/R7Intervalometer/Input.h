#pragma once
#include <stdint.h>

// Hardware-independent input logic, tested with recorded-style bounce sequences.
struct ButtonEvents { bool pressed=false, released=false, click=false, hold=false; };
class DebouncedButton {
public:
  // True from the first raw down sample through the debounced release.
  bool busy() const {return raw || stable;}
  bool rawDown() const {return raw;}
  bool stableDown() const {return stable;}
  bool isArmed() const {return armed;}
  // Read-only hint for ending a display transfer before handling input in loop().
  bool needsService(bool down,uint32_t now,uint32_t pressDebounce=30) const {
    return down!=raw || (raw!=stable && now-changed>=(raw?pressDebounce:30)) ||
           (armed && stable && raw && !sent && now-pressAt>=2000);
  }
  void begin(bool down,uint32_t now) {
    raw=stable=down;changed=pressAt=now;sent=false;
    armed=!down; // a button held at boot must be released before it can act
  }
  ButtonEvents update(bool down,uint32_t now,uint32_t pressDebounce=30) {
    ButtonEvents e;
    if(down!=raw){raw=down;changed=now;}
    if(raw!=stable && now-changed>=(raw?pressDebounce:30)) {
      stable=raw;
      if(stable){pressAt=now;sent=false;e.pressed=armed;}
      else {
        e.released=true;
        // Use physical release time, not the end of release debounce.
        if(armed && !sent) {
          if(changed-pressAt>=2000)e.hold=true;
          else e.click=true;
        }
        armed=true;
      }
    }
    // A release being debounced must never turn into a long press.
    if(armed && stable && raw && !sent && now-pressAt>=2000) {
      sent=true;e.hold=true;
    }
    return e;
  }
private:
  bool raw=false,stable=false,sent=false,armed=true;
  uint32_t changed=0,pressAt=0;
};

inline bool redrawDue(bool lit,bool dirty,bool running,uint32_t now,uint32_t last) {
  // Dirty UI updates capped at 25 Hz; countdown refresh remains 5 Hz.
  return lit && now-last>=40 && (dirty || (running && now-last>=200));
}
