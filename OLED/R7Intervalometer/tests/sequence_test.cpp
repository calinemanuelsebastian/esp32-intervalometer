#include <cassert>
#include <cstdint>
#include <iostream>
#include "../Sequence.h"
int main() {
  Settings s;s.exposure=2;s.gap=3;s.frames=2;s.delay=0;
  Sequence q;q.start(s,100);
  assert(q.focus && !q.shutter);
  q.tick(599);assert(!q.shutter);
  q.tick(600);assert(q.shutter && q.focus);
  q.tick(2599);assert(q.shutter);
  q.tick(2600);assert(!q.shutter && !q.focus && q.completed==1);
  q.tick(5099);assert(!q.focus);
  q.tick(5100);assert(q.focus && !q.shutter);
  q.tick(5599);assert(!q.shutter);
  q.tick(5600);assert(q.shutter); // exactly 3 seconds closed
  q.tick(7600);assert(q.phase==Phase::Done && !q.shutter && !q.focus && q.completed==2);
  q.tick(20000);assert(!q.shutter && q.completed==2);
  q.start(s,0);q.tick(500);q.pause();q.tick(2499);assert(q.shutter);
  q.tick(2500);assert(q.phase==Phase::Paused && !q.shutter && q.completed==1);
  q.resume(10000);q.tick(12500);q.tick(12999);assert(!q.shutter);q.tick(13000);assert(q.shutter);
  q.stop();assert(!q.active() && !q.shutter && !q.focus);
  q.tick(99999);assert(!q.shutter);
  q.start(s,0);q.pause();assert(q.phase==Phase::Paused && !q.focus);
  s.frames=0;q.start(s,0);q.tick(500);q.tick(2500);q.tick(5000);q.tick(5500);q.tick(7500);
  assert(q.completed==2 && q.active());
  // millis wrap during an exposure and during a gap.
  q.start(s,UINT32_MAX-1000U);q.tick(UINT32_MAX-500U);assert(q.shutter);
  q.tick(1498);assert(q.shutter);q.tick(1499);assert(!q.shutter);
  q.tick(3999);q.tick(4498);assert(!q.shutter);q.tick(4499);assert(q.shutter);
  // A stalled loop must close the exposure on return, not launch catch-up shots.
  q.tick(99999);assert(!q.shutter && q.phase==Phase::Gap);
  q.tick(99999);assert(!q.shutter);
  // A late call must preserve 500 ms between requesting focus and shutter.
  s.delay=2;q.start(s,0);q.tick(2000);
  assert(q.focus && !q.shutter && q.remaining(2000)==500);
  q.tick(2499);assert(!q.shutter);q.tick(2500);assert(q.shutter);
  q.pause();q.tick(4500);assert(q.phase==Phase::Paused && !q.pauseRequested);
  // Late gap service extends the closed gap rather than skipping the focus lead.
  q.resume(5000);q.tick(9000);assert(q.focus && !q.shutter);
  q.tick(9499);assert(!q.shutter);q.tick(9500);assert(q.shutter);
  // Finishing the final frame overrides an outstanding pause request.
  s.frames=1;s.delay=0;q.start(s,0);q.tick(500);q.pause();q.tick(2500);
  assert(q.phase==Phase::Done && !q.pauseRequested);
  // Cancelling a pending pause leaves normal sequencing intact.
  s.frames=2;q.start(s,0);q.tick(500);q.pause();q.pauseRequested=false;q.tick(2500);
  assert(q.phase==Phase::Gap && !q.shutter);
  // At maximum duration, arithmetic still fits in uint32_t and time rollover is safe.
  s.exposure=86400;q.start(s,UINT32_MAX-100);q.tick(399);
  assert(q.shutter && q.remaining(399)==86400000U);
  q.tick(86400398U);assert(q.shutter);q.tick(86400399U);assert(!q.shutter);
  Settings bad=s;bad.gap=0;assert(!validSettings(bad));assert(validSettings(s));
  std::cout<<"PASS: shutter boundaries, closed gap, wake lead, finite/unlimited, pause/resume, stop, rollover, delayed loop, minimum wake lead, pause flag, maximum exposure, settings bounds\n";
}
