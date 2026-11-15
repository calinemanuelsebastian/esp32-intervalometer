#pragma once
#include "Arduino.h"
inline bool testCanvasMissing=false;
struct GFXcanvas16 {
 std::vector<uint16_t> buffer;std::vector<std::string> text;
 GFXcanvas16(int w,int h):buffer(w*h){}
 uint16_t* getBuffer(){return testCanvasMissing?nullptr:buffer.data();}
 void setTextWrap(bool){} void setTextColor(uint16_t){} void setTextSize(uint8_t){} void setCursor(int,int){}
 void print(const char* s){text.push_back(s);}
 void fillScreen(uint16_t c){std::fill(buffer.begin(),buffer.end(),c);text.clear();}
 template<class... T> void drawFastHLine(T...){}
 template<class... T> void fillRoundRect(T...){}
 template<class... T> void fillCircle(T...){}
 void fillRect(int x,int y,int w,int h,uint16_t c){
   for(int row=std::max(0,y);row<std::min(80,y+h);row++)
     for(int col=std::max(0,x);col<std::min(160,x+w);col++)buffer[row*160+col]=c;
 }
 template<class... T> void drawRect(T...){}
};
