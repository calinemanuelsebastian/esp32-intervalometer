#include "PulseTimer.h"

// Out-of-line definitions keep Xtensa IRAM literal pools with their functions.
// ARDUINO_ISR_ATTR can be empty in a precompiled core; IRAM_ATTR is explicit.
void IRAM_ATTR PulseTimer::setPins(bool shutter,bool focus) {
#if defined(ARDUINO_ARCH_ESP32)
  if(!shutter)gpio_ll_set_level(&GPIO,shutterPin,LOW);
  gpio_ll_set_level(&GPIO,focusPin,focus?HIGH:LOW);
  if(shutter)gpio_ll_set_level(&GPIO,shutterPin,HIGH);
#else
  if(!shutter)gpio_set_level((gpio_num_t)shutterPin,LOW);
  gpio_set_level((gpio_num_t)focusPin,focus?HIGH:LOW);
  if(shutter)gpio_set_level((gpio_num_t)shutterPin,HIGH);
#endif
}
// No flash calls between this final time check and the GPIO assertion/unlock.
bool IRAM_ATTR PulseTimer::commitStart(uint64_t referenceUs,uint64_t durationUs,bool dryRun) {
  portENTER_CRITICAL(&mux);
  uint64_t now=esp_timer_get_time(),setupUs=now-referenceUs;
  bool ok=arming && setupUs<=ARM_BUDGET_US && setupUs<durationUs;
  if(ok) {startedAtUs=now;active=true;setPins(!dryRun,!dryRun);}
  arming=false;
  portEXIT_CRITICAL(&mux);
  return ok;
}
bool IRAM_ATTR PulseTimer::alarm(gptimer_handle_t,
      const gptimer_alarm_event_data_t* event,void* context) {
    auto* self=static_cast<PulseTimer*>(context);
    portENTER_CRITICAL_ISR(&self->mux);
    // IDF reads the alarm value when servicing the IRQ, so a stale interrupt
    // can carry a newly programmed alarm value. Check its captured time too.
    if(self->active && event->alarm_value==self->deadline &&
       event->count_value>=self->deadline) {
      self->setPins(false,false);
      // Both the clock function and its hardware-counter helper are in IRAM.
      // gptimer_get_raw_count() and millis() are in flash in the stock core;
      // neither is safe here, even AFTER the falling edge has been written.
      self->closedAtUs=esp_timer_get_time();
      self->reading.elapsedUs=self->closedAtUs-self->startedAtUs;
      self->reading.measured=true;
      self->reading.irqLateUs=event->count_value-self->deadline;
      self->reading.valid=self->reading.complete=true;
      self->active=false;self->pending=true;
    }
    portEXIT_CRITICAL_ISR(&self->mux);
    return false;
  }
