#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include "TimerPlatform.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <hal/gpio_ll.h>
#include <soc/gpio_struct.h>
#endif

struct PulseReading {
  uint64_t requestedUs=0, elapsedUs=0, irqLateUs=0;
  bool valid=false, measured=false, complete=false, dryRun=true, timedOut=false;
};

// A continuously running 1 MHz timer owns the end of a timed trigger.
// No display, Serial, allocation or flash writes occur in the callback.
// GPIO timestamps describe the command, NOT the camera's sensor exposure.
class PulseTimer {
public:
  PulseTimer(int shutterPin,int focusPin):shutterPin(shutterPin),focusPin(focusPin){}
  bool begin() {
    if(timer)return ready; // setup is normally called only once
    gptimer_config_t config={};
    config.clk_src=GPTIMER_CLK_SRC_APB;
    config.direction=GPTIMER_COUNT_UP;
    config.resolution_hz=1000000;
    config.intr_priority=3;
    if(gptimer_new_timer(&config,&timer)!=ESP_OK)return false;
    gptimer_event_callbacks_t callbacks={};callbacks.on_alarm=alarm;
    if(gptimer_register_event_callbacks(timer,&callbacks,this)!=ESP_OK ||
       !configureReleaseInterrupt(timer,this) ||
       gptimer_enable(timer)!=ESP_OK) {
      gptimer_del_timer(timer);timer=nullptr;return false;
    }
    uint32_t resolution=0;
    if(gptimer_get_resolution(timer,&resolution)!=ESP_OK || resolution!=1000000 ||
       gptimer_start(timer)!=ESP_OK) {
      gptimer_disable(timer);gptimer_del_timer(timer);timer=nullptr;return false;
    }
    ready=true;return true;
  }
  bool healthy() const {return ready;}
  static constexpr uint64_t ARM_BUDGET_US=1000;
  static constexpr uint64_t RELEASE_GRACE_US=20000;

  // All foreground methods have one owner (the application loop). Only alarm()
  // runs concurrently. No driver API is called with this object's mux held.
  bool needsService() {
    portENTER_CRITICAL(&mux);
    bool result=pending || (active && (uint64_t)esp_timer_get_time()-startedAtUs>=reading.requestedUs+RELEASE_GRACE_US);
    portEXIT_CRITICAL(&mux);return result;
  }
  // Foreground fail-safe for a missing callback. This is NOT a second precision
  // timer: it needs a running loop, stops the sequence and latches a timer fault.
  bool serviceDeadline() {
    portENTER_CRITICAL(&mux);
    uint64_t now=esp_timer_get_time();
    bool expired=active && now-startedAtUs>=reading.requestedUs+RELEASE_GRACE_US;
    if(expired) {
      setPins(false,false);
      reading.elapsedUs=(uint64_t)esp_timer_get_time()-startedAtUs;
      reading.valid=reading.measured=reading.timedOut=true;reading.complete=false;
      active=arming=pending=false;ready=false;
    }
    portEXIT_CRITICAL(&mux);
    if(expired)gptimer_set_alarm_action(timer,nullptr);
    return expired;
  }

  bool start(uint64_t durationUs,bool dryRun) {
    if(!ready || !durationUs || durationUs>86400000000ULL)return false;
    portENTER_CRITICAL(&mux);
    if(active || pending || arming) {portEXIT_CRITICAL(&mux);return false;}
    arming=true;
    reading=PulseReading();reading.requestedUs=durationUs;reading.dryRun=dryRun;
    portEXIT_CRITICAL(&mux);

    // Shutter stays LOW throughout driver calls. A preempted setup can retry
    // with a fresh deadline, but never indefinitely or with the shutter asserted.
    for(unsigned attempt=0;attempt<3;attempt++) {
      portENTER_CRITICAL(&mux);arming=true;portEXIT_CRITICAL(&mux);
      uint64_t referenceUs=esp_timer_get_time(),counter=0,armedAt=0;
      bool ok=gptimer_get_raw_count(timer,&counter)==ESP_OK;
      portENTER_CRITICAL(&mux);deadline=counter+durationUs;portEXIT_CRITICAL(&mux);
      gptimer_alarm_config_t alarmConfig={};alarmConfig.alarm_count=deadline;
      if(ok)ok=gptimer_set_alarm_action(timer,&alarmConfig)==ESP_OK;
      if(ok)ok=gptimer_get_raw_count(timer,&armedAt)==ESP_OK && armedAt<deadline;
      if(!ok)break;
      if(commitStart(referenceUs,durationUs,dryRun))return true;
      if(gptimer_set_alarm_action(timer,nullptr)!=ESP_OK)break;
    }
    portENTER_CRITICAL(&mux);
    setPins(false,false);active=arming=false;ready=false;
    portEXIT_CRITICAL(&mux);
    gptimer_set_alarm_action(timer,nullptr);
    return false;
  }

  void untimed(bool shutter,bool focus) {
    portENTER_CRITICAL(&mux);
    bool cancel=active || arming;
    if(active) {
      setPins(false,false);
      reading.elapsedUs=(uint64_t)esp_timer_get_time()-startedAtUs;
      reading.measured=reading.valid=true;reading.complete=false;
    }
    active=arming=pending=false; // queued callbacks cannot touch a later manual hold
    portEXIT_CRITICAL(&mux);
    bool ok=!cancel || gptimer_set_alarm_action(timer,nullptr)==ESP_OK;
    portENTER_CRITICAL(&mux);
    if(!ok)ready=false;
    setPins(ready && shutter,ready && focus);
    portEXIT_CRITICAL(&mux);
  }
  bool takeCompletion(uint32_t& closedAt) {
    portENTER_CRITICAL(&mux);
    bool result=pending;
    uint64_t stamp=closedAtUs;
    if(result)pending=false;
    portEXIT_CRITICAL(&mux);
    if(result)closedAt=(uint32_t)(stamp/1000);
    return result;
  }
  PulseReading last() {
    portENTER_CRITICAL(&mux);
    PulseReading result=reading;
    portEXIT_CRITICAL(&mux);
    return result;
  }
private:
  const int shutterPin,focusPin;
  gptimer_handle_t timer=nullptr;
  portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
  bool ready=false,active=false,pending=false,arming=false;
  uint64_t deadline=0;
  uint64_t startedAtUs=0,closedAtUs=0;
  PulseReading reading;

  bool commitStart(uint64_t referenceUs,uint64_t durationUs,bool dryRun);
  void setPins(bool shutter,bool focus);
  static bool alarm(gptimer_handle_t timer,
      const gptimer_alarm_event_data_t* event,void* context);
};
