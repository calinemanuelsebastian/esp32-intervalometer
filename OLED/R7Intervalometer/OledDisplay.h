#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "DisplayConfig.h"

// Checked I2C transport for 128x64 SSD1306 (or explicitly selected SH1106).
// GFXcanvas1 is row-packed; these controllers expect vertical page-packed bytes.
// Small transactions let a read-only callback yield the frame to the main loop.
class OledDisplay {
public:
  explicit OledDisplay(bool sh1106=OLED_SH1106):sh1106(sh1106){}
  bool healthy() const {return ready;}
  bool interrupted() const {return frameInterrupted;}
  uint8_t address() const {return addr;}
  uint8_t error() const {return lastError;}
  const char* controller() const {return sh1106?"SH1106":"SSD1306";}
  bool begin(uint8_t contrast) {
    ready=false;powered=false;addr=0;lastError=0;
    Wire.end();
    if(!Wire.begin(OLED_SDA,OLED_SCL,OLED_I2C_HZ)){lastError=255;return false;}
    Wire.setTimeOut(OLED_TIMEOUT_MS);
    delay(20); // allow a power-on-reset module to settle
    const uint8_t candidates[]={0x3C,0x3D};
    for(uint8_t candidate:candidates) {
      if(OLED_ADDRESS && candidate!=OLED_ADDRESS)continue;
      Wire.beginTransmission(candidate);lastError=Wire.endTransmission();
      if(!lastError){addr=candidate;break;}
    }
    if(!addr)return false;
    ready=true;
    const uint8_t common1[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40};
    if(!send(0x00,common1,sizeof(common1)))return false;
    if(sh1106) {
      const uint8_t pump[]={0xAD,0x8B,0x33}; // SH1106 DC/DC and pump voltage
      if(!send(0x00,pump,sizeof(pump)))return false;
    } else {
      const uint8_t pump[]={0x8D,0x14,0x20,0x02,0x2E}; // pump, page mode, no scroll
      if(!send(0x00,pump,sizeof(pump)))return false;
    }
    const uint8_t common2[]={0xA1,0xC8,0xDA,0x12,0x81,contrast,
      0xD9,uint8_t(sh1106?0x1F:0xF1),0xDB,0x40,0xA4,0xA6};
    if(!send(0x00,common2,sizeof(common2)))return false;
    lastContrast=contrast;
    return true; // remains off until the application has written its first frame
  }
  bool power(bool on) {
    if(!ready)return false;
    if(powered==on)return true;
    uint8_t cmd=on?0xAF:0xAE;
    if(!send(0x00,&cmd,1))return false;
    powered=on;return true;
  }
  bool contrast(uint8_t value) {
    if(!ready)return false;
    if(value==lastContrast)return true;
    uint8_t cmd[]={0x81,value};
    if(!send(0x00,cmd,2))return false;
    lastContrast=value;return true;
  }
  bool present(const uint8_t* rows,bool (*continueFrame)()=nullptr) {
    frameInterrupted=false;
    if(!ready || !rows)return false;
    const bool beganOn=powered;
    for(int page=0;page<8;page++) {
      uint8_t pos[]={uint8_t(0xB0+page),uint8_t(sh1106?0x02:0x00),0x10};
      if(!send(0x00,pos,3))return false;
      if(continueFrame && !continueFrame()){frameInterrupted=true;return true;}
      if(!ready)return false;
      if(beganOn && !powered)return true;
      for(int x=0;x<128;x+=16) {
        uint8_t bytes[16]={};
        for(int i=0;i<16;i++)for(int bit=0;bit<8;bit++) {
          if(rows[(page*8+bit)*16+(x+i)/8] & (0x80>>((x+i)&7)))bytes[i]|=1<<bit;
        }
        if(!send(0x40,bytes,sizeof(bytes)))return false;
        if(continueFrame && !continueFrame()){frameInterrupted=true;return true;}
        if(!ready)return false;
        if(beganOn && !powered)return true;
      }
    }
    return true;
  }
private:
  bool sh1106,ready=false,powered=false,frameInterrupted=false;
  uint8_t addr=0,lastError=0,lastContrast=96;
  bool send(uint8_t control,const uint8_t* data,size_t n) {
    if(!ready)return false;
    Wire.beginTransmission(addr);
    bool written=Wire.write(control)==1;
    written=Wire.write(data,n)==n && written;
    lastError=Wire.endTransmission();
    if(!written && !lastError)lastError=254;
    if(lastError){ready=false;return false;}
    return true;
  }
};
