#pragma once
#include "Arduino.h"
#include "ClassicFont.h"
#include <cassert>
inline bool testCanvasMissing=false;
struct GFXcanvas1 {
 std::vector<uint8_t> buffer;std::vector<std::string> text;
 int w,h,x=0,y=0;uint8_t size=1;uint16_t color=1;
 GFXcanvas1(int width,int height):buffer((width+7)/8*height),w(width),h(height){}
 uint8_t* getBuffer(){return testCanvasMissing?nullptr:buffer.data();}
 void setTextWrap(bool){} void setTextColor(uint16_t c){color=c;} void setTextSize(uint8_t n){size=n;} void setCursor(int a,int b){x=a;y=b;}
 void drawPixel(int px,int py,uint16_t c){if(px<0 || py<0 || px>=w || py>=h)return;auto &b=buffer[py*((w+7)/8)+px/8];uint8_t mask=0x80>>(px&7);if(c)b|=mask;else b&=~mask;}
 void fillRect(int px,int py,int width,int height,uint16_t c){for(int j=0;j<height;j++)for(int i=0;i<width;i++)drawPixel(px+i,py+j,c);}
 void print(const char* s){
   text.push_back(s);assert(x>=0 && y>=0 && x+(int)strlen(s)*6*size<=w && y+8*size<=h);
   for(const char* p=s;*p;p++){for(int i=0;i<5;i++){uint8_t bits=font[(uint8_t)*p*5+i];for(int j=0;j<8;j++)if(bits&(1<<j))fillRect(x+i*size,y+j*size,size,size,color);}x+=6*size;}
 }
 void fillScreen(uint16_t c){std::fill(buffer.begin(),buffer.end(),c?255:0);text.clear();}
 void drawFastHLine(int px,int py,int n,uint16_t c){fillRect(px,py,n,1,c);}
 void drawRect(int px,int py,int width,int height,uint16_t c){fillRect(px,py,width,1,c);fillRect(px,py+height-1,width,1,c);fillRect(px,py,1,height,c);fillRect(px+width-1,py,1,height,c);}
 void fillCircle(int px,int py,int r,uint16_t c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)drawPixel(px+i,py+j,c);}
 void drawCircle(int px,int py,int r,uint16_t c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r && i*i+j*j>=(r-1)*(r-1))drawPixel(px+i,py+j,c);}
};
