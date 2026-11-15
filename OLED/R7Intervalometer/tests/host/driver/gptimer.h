#pragma once
#include "Arduino.h"
#include "gpio.h"
// Deterministic peripheral simulation. Alarms run during delay()/SPI mock
// transfers or explicit time advances, without calling application loop().
using esp_err_t=int;
#define ESP_FAIL -1
#define GPTIMER_CLK_SRC_APB 1
#define GPTIMER_COUNT_UP 1
struct gptimer_config_t {int clk_src=0,direction=0;uint32_t resolution_hz=0;int intr_priority=0;};
struct gptimer_alarm_config_t {uint64_t alarm_count=0,reload_count=0;};
struct gptimer_alarm_event_data_t {uint64_t count_value=0,alarm_value=0;};
struct FakeTimer;
using gptimer_handle_t=FakeTimer*;
using gptimer_alarm_cb_t=bool(*)(gptimer_handle_t,const gptimer_alarm_event_data_t*,void*);
struct gptimer_event_callbacks_t {gptimer_alarm_cb_t on_alarm=nullptr;};
struct FakeTimer {
  bool enabled=false,running=false,armed=false,iram=false;
  uint64_t deadline=0,origin=0;
  uint32_t frequency=0;
  gptimer_alarm_cb_t callback=nullptr;void* context=nullptr;
};
inline FakeTimer testTimer;
inline bool testFailCreate=false,testFailArm=false,testFailRead=false;
inline uint64_t testTimerLatencyUs=0;
inline uint64_t testArmDelayUs=0;
inline bool testDropAlarm=false;
inline void (*testArmHook)()=nullptr;
// Model flash-cache suspension separately from global interrupt masking.
// IRAM interrupts bypass only the former, never testTimerLatencyUs.
inline uint64_t testCacheOffStartUs=0,testCacheOffEndUs=0;
inline void testTimerAdvance(uint64_t us) {
  uint64_t target=testClockUs+us;
  uint64_t alarmAt=testTimer.origin+testTimer.deadline+testTimerLatencyUs;
  if(!testTimer.iram && alarmAt>=testCacheOffStartUs && alarmAt<testCacheOffEndUs)
    alarmAt=testCacheOffEndUs;
  if(testTimer.enabled && testTimer.running && testTimer.armed && alarmAt<=target) {
    testClockUs=std::max(testClockUs,alarmAt);testNow=(uint32_t)(testClockUs/1000);
    testTimer.armed=false;
    gptimer_alarm_event_data_t event{testClockUs-testTimer.origin,testTimer.deadline};
    if(!testDropAlarm)testTimer.callback(&testTimer,&event,testTimer.context);
  }
  testClockUs=target;testNow=(uint32_t)(testClockUs/1000);
}
inline esp_err_t gptimer_new_timer(const gptimer_config_t* config,gptimer_handle_t* timer) {
  if(testFailCreate)return ESP_FAIL;
  testTimer=FakeTimer();testTimer.origin=testClockUs;testTimer.frequency=config->resolution_hz;
  *timer=&testTimer;testAdvanceHook=testTimerAdvance;return ESP_OK;
}
inline esp_err_t gptimer_register_event_callbacks(gptimer_handle_t t,const gptimer_event_callbacks_t* c,void* ctx){t->callback=c->on_alarm;t->context=ctx;return ESP_OK;}
inline esp_err_t gptimer_enable(gptimer_handle_t t){t->enabled=true;return ESP_OK;}
inline esp_err_t gptimer_disable(gptimer_handle_t t){t->enabled=false;return ESP_OK;}
inline esp_err_t gptimer_del_timer(gptimer_handle_t){return ESP_OK;}
inline esp_err_t gptimer_start(gptimer_handle_t t){t->running=true;return ESP_OK;}
inline esp_err_t gptimer_get_resolution(gptimer_handle_t t,uint32_t* out){*out=t->frequency;return ESP_OK;}
inline esp_err_t gptimer_get_raw_count(gptimer_handle_t t,uint64_t* out){assert(testCriticalDepth==0);if(testFailRead)return ESP_FAIL;*out=testClockUs-t->origin;return ESP_OK;}
inline esp_err_t gptimer_set_alarm_action(gptimer_handle_t t,const gptimer_alarm_config_t* c) {
  assert(testCriticalDepth==0);
  if(c && testArmHook)testArmHook();
  if(testFailArm && c)return ESP_FAIL;
  t->armed=c!=nullptr;if(c){t->deadline=c->alarm_count;if(testArmDelayUs)testAdvanceUs(testArmDelayUs);}return ESP_OK;
}
