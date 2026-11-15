#pragma once
#include <stdint.h>

struct Settings {
  uint32_t magic = 0x52374932;
  uint32_t exposure = 90;       // seconds
  uint32_t gap = 3;             // shutter closed, seconds
  uint32_t frames = 20;         // 0 = unlimited
  uint32_t delay = 5;           // initial delay, seconds
  uint32_t lightTimeout = 10;   // seconds after interaction while exposing
  uint32_t dryRun = 1;          // simulate without activating either optocoupler
};
inline bool validSettings(const Settings& s) {
  return s.magic == 0x52374932 && s.exposure <= 86400 &&
    s.gap >= 1 && s.gap <= 3600 && s.frames <= 9999 && s.delay <= 3600 &&
    s.lightTimeout >= 1 && s.lightTimeout <= 120 && s.dryRun <= 1;
}

enum class Phase { Idle, Delay, Exposing, Gap, Paused, Done, Stopped };

// No Arduino dependencies: timing/stop/pause behaviour is host-testable.
class Sequence {
public:
  explicit Sequence(bool hardwareTimed=false):externalTiming(hardwareTimed){}
  const bool externalTiming;
  Phase phase = Phase::Idle;
  Settings plan;
  uint32_t completed = 0;
  bool pauseRequested = false;
  bool shutter = false, focus = false;
  uint32_t since = 0, duration = 0, focusSince = 0;
  uint32_t triggerDurationMs=90000;
  bool cameraTimed=false;
  static constexpr uint32_t WAKE_MS = 500;

  bool active() const {
    return phase == Phase::Delay || phase == Phase::Exposing ||
           phase == Phase::Gap || phase == Phase::Paused;
  }
  uint32_t remaining(uint32_t now) const {
    uint32_t elapsed = now - since;
    uint32_t left = elapsed >= duration ? 0 : duration - elapsed;
    if ((phase == Phase::Delay || phase == Phase::Gap) && focus) {
      uint32_t awake = now - focusSince;
      uint32_t wakeLeft = awake >= WAKE_MS ? 0 : WAKE_MS - awake;
      if (wakeLeft > left) left = wakeLeft;
    }
    return left;
  }
  void start(const Settings& s, uint32_t now, uint32_t commandMs=0,bool cameraMode=false) {
    plan = s; completed = 0; pauseRequested = false;
    triggerDurationMs=commandMs?commandMs:(s.exposure?s.exposure*1000U:100U);
    cameraTimed=cameraMode;
    shutter = focus = false;
    enter(Phase::Delay, now, s.delay ? s.delay * 1000U : WAKE_MS);
    tick(now);
  }
  void stop() {
    shutter = focus = false; pauseRequested = false;
    phase = Phase::Stopped; duration = 0;
  }
  void pause() {
    if (phase == Phase::Exposing) pauseRequested = true;
    else if (active()) {
      shutter = focus = false; pauseRequested = false; phase = Phase::Paused; duration = 0;
    }
  }
  void resume(uint32_t now) {
    if (phase != Phase::Paused) return;
    pauseRequested = false;
    enter(Phase::Gap, now, plan.gap * 1000U);
  }
  void tick(uint32_t now) {
    if (phase == Phase::Delay || phase == Phase::Gap) {
      uint32_t elapsed = now - since;
      uint32_t scheduledLeft = elapsed >= duration ? 0 : duration - elapsed;
      if (!focus && scheduledLeft <= WAKE_MS) {focus = true; focusSince = now;}
      // A late loop must still allow the half-press lead, not assert both together.
      if (!scheduledLeft && focus && now - focusSince >= WAKE_MS) {
        shutter = focus = true;
        enter(Phase::Exposing, now, triggerDurationMs);
      }
    } else if (!externalTiming && phase == Phase::Exposing && !remaining(now)) finishExposure(now);
  }
  // Hardware-timed users call this with the actual interrupt release timestamp.
  void finishExposure(uint32_t closedAt) {
      if(phase!=Phase::Exposing)return;
      shutter = focus = false; ++completed;
      if (plan.frames && completed >= plan.frames) {
        phase = Phase::Done; duration = 0; pauseRequested = false;
      } else if (pauseRequested) {
        phase = Phase::Paused; duration = 0; pauseRequested = false;
      } else enter(Phase::Gap, closedAt, plan.gap * 1000U);
  }
private:
  void enter(Phase p, uint32_t now, uint32_t ms) {
    phase = p; since = now; duration = ms;
  }
};
