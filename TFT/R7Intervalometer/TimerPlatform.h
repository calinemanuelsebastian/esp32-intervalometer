#pragma once
#include <driver/gptimer.h>
#include <esp_intr_alloc.h>
#include <esp_private/gptimer.h>
#include <esp_timer.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_idf_version.h>
#include <esp_arduino_version.h>
#include <esp_memory_utils.h>
#include <sdkconfig.h>
// This adapter uses one private IDF accessor, never an opaque-handle cast.
// Re-audit the driver ISR and its callees before changing this build pin.
#if ESP_ARDUINO_VERSION != ESP_ARDUINO_VERSION_VAL(3, 3, 12) || \
    ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "Timing release requires Espressif esp32 board package 3.3.12 (IDF 5.5.5)."
#endif
#if !CONFIG_IDF_TARGET_ESP32 || !CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM || \
    !CONFIG_GPTIMER_OBJ_CACHE_SAFE || !CONFIG_ESP_TIMER_IN_IRAM
#error "Timing release requires the audited classic ESP32 IRAM/DRAM driver configuration."
#endif
#endif

// Stock Arduino enables the driver's IRAM placement but does NOT allocate its
// IRQ with ESP_INTR_FLAG_IRAM. Mark this dedicated (non-shared) interrupt before
// enabling it, so flash-cache suspension does not postpone shutter release.
// Fail closed if this cannot be established; see RELEASE-1.10.txt.
inline bool configureReleaseInterrupt(gptimer_handle_t timer,void* context) {
#if defined(ARDUINO_ARCH_ESP32)
  if(!esp_ptr_internal(timer) || !esp_ptr_internal(context))return false;
#else
  (void)context;
#endif
  intr_handle_t interrupt=nullptr;
  return gptimer_get_intr_handle(timer,&interrupt)==ESP_OK && interrupt &&
         esp_intr_set_in_iram(interrupt,true)==ESP_OK;
}
