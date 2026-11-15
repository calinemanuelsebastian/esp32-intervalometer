#pragma once
#include "Arduino.h"
using gpio_num_t=int;
using esp_err_t=int;
#define ESP_OK 0
struct gpio_io_config_t {bool pu=false,pd=false,ie=false,oe=false;};
inline int gpio_set_level(gpio_num_t p,int v){outputLevels[p]=v;return ESP_OK;}
inline int gpio_get_io_config(gpio_num_t p,gpio_io_config_t* c){c->pu=modes[p]==INPUT_PULLUP;c->pd=false;c->ie=modes[p]==INPUT || modes[p]==INPUT_PULLUP;c->oe=modes[p]==OUTPUT;return ESP_OK;}
