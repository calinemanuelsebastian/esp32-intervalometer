#pragma once
#include <stdint.h>

// Separate record keeps the existing seconds-based settings readable.
struct CaptureSettings {
  uint32_t magic=0x52374331, mode=0, fractionMs=0, triggerMs=100;
};
inline bool validCaptureSettings(const CaptureSettings& s) {
  return s.magic==0x52374331 && s.mode<=1 && s.fractionMs<=999 &&
    s.triggerMs>=50 && s.triggerMs<=1000;
}
inline uint32_t bulbDurationMs(uint32_t seconds,uint32_t fraction) {
  uint64_t ms=(uint64_t)seconds*1000+fraction;
  return ms<100?100:ms>86400000?86400000:(uint32_t)ms;
}

// Foreground aggregation: no Serial/flash operations inside timer interrupts.
struct TimingStats {
  uint32_t count=0,missing=0;
  uint64_t requestedUs=0,minUs=0,maxUs=0,maxIrqLateUs=0;
  bool dryRun=true;
  void reset(){*this=TimingStats();}
  void add(uint64_t requested,uint64_t actual,uint64_t irqLate,bool measured,bool dry) {
    if(!measured){++missing;return;}
    if(count && (requested!=requestedUs || dry!=dryRun))reset();
    requestedUs=requested;dryRun=dry;
    if(!count || actual<minUs)minUs=actual;
    if(!count || actual>maxUs)maxUs=actual;
    if(irqLate>maxIrqLateUs)maxIrqLateUs=irqLate;
    ++count;
  }
};
