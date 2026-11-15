#pragma once
#include "Arduino.h"
struct WirePacket {uint8_t address;std::vector<uint8_t> bytes;};
inline bool testClobberPullup=false,testWireBeginFail=false,testWireStuck=false,testWireShortWrite=false;
inline uint8_t testOledAddress=0x3C;
inline int testWireFailAfter=-1;
inline void (*testWireHook)()=nullptr;
struct WireMock {
 int sda=-1,scl=-1,transactions=0;uint32_t frequency=0;uint16_t timeout=0;
 uint8_t target=0;std::vector<uint8_t> bytes;std::vector<WirePacket> packets;
 void end(){}
 bool begin(int a,int b,uint32_t hz){sda=a;scl=b;frequency=hz;if(testClobberPullup)pinMode(27,INPUT);return !testWireBeginFail;}
 void setTimeOut(uint16_t n){timeout=n;}
 void beginTransmission(uint8_t a){target=a;bytes.clear();}
 size_t write(uint8_t v){bytes.push_back(v);return testWireShortWrite?0:1;}
 size_t write(const uint8_t* b,size_t n){bytes.insert(bytes.end(),b,b+n);return testWireShortWrite?0:n;}
 uint8_t endTransmission(){
   ++transactions;packets.push_back({target,bytes});
   bool fail=testWireStuck || (testWireFailAfter>=0 && transactions>=testWireFailAfter);
   uint8_t result=fail?5:target!=testOledAddress?2:0;
   testAdvanceUs(fail?timeout*1000:bytes.size()>1 && bytes[0]==0x40?375:250);
   if(testWireHook)testWireHook();
   return result;
 }
};
inline WireMock Wire;
