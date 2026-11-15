#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include "../Exposure.h"
int main() {
  CameraSpeedSettings s;char b[32];
  assert(validCameraSpeed(s));formatCameraSpeed(cameraSpeedIndex(s),b,sizeof(b));assert(!strcmp(b,"1/100 s"));
  for(uint32_t i=0;i<=CAMERA_SPEED_MAX;i++) {
    s=cameraSpeedAt(i);assert(validCameraSpeed(s) && cameraSpeedIndex(s)==i);
    formatCameraSpeed(i,b,sizeof(b));assert(strlen(b)<=8);
  }
  s.denominator=60;s.seconds=0;formatCameraSpeed(cameraSpeedIndex(s),b,sizeof(b));assert(!strcmp(b,"1/60 s"));
  s.denominator=30;formatCameraSpeed(cameraSpeedIndex(s),b,sizeof(b));assert(!strcmp(b,"1/30 s"));
  s.seconds=1;assert(!validCameraSpeed(s));s.denominator=0;assert(validCameraSpeed(s));
  s.seconds=31;assert(!validCameraSpeed(s));s.seconds=0;assert(!validCameraSpeed(s));
  s=CameraSpeedSettings();s.denominator=61;assert(!validCameraSpeed(s));s.magic=0;assert(!validCameraSpeed(s));
  struct Sample {uint32_t ms;const char* label;} samples[]={
    {100,"1/10 s"},{125,"1/8 s"},{200,"1/5 s"},{250,"1/4 s"},{500,"1/2 s"},
    {1000,"1 s"},{1250,"1.25 s"},{10125,"10.125 s"},{86400000,"86400 s"},{375,"0.375 s"}};
  for(const auto& item:samples){formatBulbTime(item.ms,b,sizeof(b));assert(!strcmp(b,item.label));}
  uint32_t ms=100;const uint32_t expected[]={125,200,250,500,1000,2000};
  for(uint32_t next:expected){assert(stepBulbTime(ms,1)==next && stepBulbTime(next,-1)==ms);ms=next;}
  assert(stepBulbTime(100,INT_MIN)==100 && stepBulbTime(100,INT_MAX)==86400000);
  assert(stepBulbTime(86400000,1)==86400000 && stepBulbTime(86400000,-1)==86399000);
  assert(stepBulbTime(375,0)==375 && stepBulbTime(375,1)==500 && stepBulbTime(375,-1)==250);
  assert(stepBulbTime(1250,0)==1250 && stepBulbTime(1250,1)==2000 && stepBulbTime(1250,-1)==1000);
  assert(stepBulbTime(1000,-100000)==100 && stepBulbTime(90000,2)==92000);
  std::cout<<"PASS: fractional labels, exact custom times, exposure selection bounds and camera reminder values\n";
}
