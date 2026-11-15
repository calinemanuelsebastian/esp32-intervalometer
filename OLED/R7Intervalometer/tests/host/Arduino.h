#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#define ARDUINO_ISR_ATTR
#define IRAM_ATTR
#define DRAM_ATTR
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 2
#define INPUT_PULLUP 5
#define CHANGE 3
inline int testCriticalDepth=0;
inline bool testInDisplayFrame=false;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x),++testCriticalDepth)
#define portEXIT_CRITICAL(x) ((void)(x),--testCriticalDepth)
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
inline uint32_t testNow=0;
inline uint64_t testClockUs=0;
inline void (*testAdvanceHook)(uint64_t)=nullptr;
inline void testAdvanceUs(uint64_t us){
  if(testAdvanceHook)testAdvanceHook(us);
  else {testClockUs+=us;testNow=(uint32_t)(testClockUs/1000);}
}
inline void testSetMillis(uint32_t ms){testAdvanceUs((uint64_t)(uint32_t)(ms-testNow)*1000);}
inline int modes[40], externalLevels[40], outputLevels[40], ignoredWrites=0;
inline bool irqEnabled[40];
inline uint32_t millis(){return testNow;}
inline uint32_t micros(){return (uint32_t)testClockUs;}
inline void delay(uint32_t ms){testAdvanceUs((uint64_t)ms*1000);}
inline void pinMode(int p,int mode){modes[p]=mode;}
inline void digitalWrite(int p,int level){if(modes[p]==OUTPUT)outputLevels[p]=level;else ignoredWrites++;}
inline int digitalRead(int p){return externalLevels[p]>=0?externalLevels[p]:(modes[p]==INPUT_PULLUP?HIGH:LOW);}
inline int digitalPinToInterrupt(int p){return p;}
inline void attachInterrupt(int p,void(*)(),int){irqEnabled[p]=true;}
inline void detachInterrupt(int p){irqEnabled[p]=false;}
struct SerialMock {
 std::string output,input;int space=128;
 void begin(int){}
 int availableForWrite(){return space;}
 int available(){return input.size();}
 char read(){char c=input[0];input.erase(0,1);return c;}
 size_t write(const uint8_t* b,size_t n){output.append((const char*)b,n);return n;}
 size_t println(const char* s){output+=s;output+='\n';return strlen(s)+1;}
 size_t printf(const char* fmt,...){char b[512];va_list a;va_start(a,fmt);int n=vsnprintf(b,sizeof(b),fmt,a);va_end(a);output+=b;return n;}
};
inline SerialMock Serial;
struct EspMock {uint32_t getFreeHeap(){return 300000;}};
inline EspMock ESP;
