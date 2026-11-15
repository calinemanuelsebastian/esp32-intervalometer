#pragma once
#include "driver/gptimer.h"
using intr_handle_t=FakeTimer*;
inline bool testFailIramMark=false;
inline esp_err_t esp_intr_set_in_iram(intr_handle_t interrupt,bool enable) {
  if(!interrupt || testFailIramMark)return ESP_FAIL;
  // Production must configure this before enabling the timer's interrupt.
  if(interrupt->enabled)return ESP_FAIL;
  interrupt->iram=enable;return ESP_OK;
}
