#pragma once
#include "esp_intr_alloc.h"
inline bool testFailIntrHandle=false;
inline esp_err_t gptimer_get_intr_handle(gptimer_handle_t timer,intr_handle_t* out) {
  if(testFailIntrHandle)return ESP_FAIL;
  if(!timer->callback)return ESP_FAIL;
  *out=timer;return ESP_OK;
}
