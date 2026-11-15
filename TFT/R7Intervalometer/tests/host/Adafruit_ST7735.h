#pragma once
#include "Arduino.h"
#define INITR_MINI160x80 4
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
inline bool testClobberPullup=false;
inline void (*testSpiHook)()=nullptr;
struct Adafruit_ST7735 {
 int cs,dc,rst,transfers=0;
 bool inverted=false;
 Adafruit_ST7735(int a,int b,int c):cs(a),dc(b),rst(c){}
 void initR(uint8_t){pinMode(cs,OUTPUT);pinMode(dc,OUTPUT);pinMode(rst,OUTPUT);delay(600);if(testClobberPullup)pinMode(27,INPUT);}
 void setRotation(uint8_t){} void setSPISpeed(uint32_t){} void invertDisplay(bool v){inverted=v;}
 void drawRGBBitmap(int,int,uint16_t*,int,int h){transfers++;testAdvanceUs(h*13000/80);if(testSpiHook)testSpiHook();}
};
