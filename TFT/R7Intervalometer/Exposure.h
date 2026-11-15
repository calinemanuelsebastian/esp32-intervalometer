#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// Camera speeds are reminders only. The remote jack cannot set shutter speed.
// Store the rational value rather than a list index so future list edits are safe.
struct CameraSpeedSettings {
  uint32_t magic=0x52375331, denominator=100, seconds=0;
};
constexpr uint32_t CAMERA_DENOMINATORS[]={8000,6400,5000,4000,3200,2500,2000,
  1600,1250,1000,800,640,500,400,320,250,200,160,125,100,80,60,50,40,30,
  25,20,15,13,10,8,6,5,4,3,2};
constexpr uint32_t CAMERA_FRACTIONS=sizeof(CAMERA_DENOMINATORS)/sizeof(CAMERA_DENOMINATORS[0]);
constexpr uint32_t CAMERA_SPEED_MAX=CAMERA_FRACTIONS+30-1; // last choice: 30 seconds
inline bool validCameraSpeed(const CameraSpeedSettings& s) {
  if(s.magic!=0x52375331)return false;
  if(s.denominator==0)return s.seconds>=1 && s.seconds<=30;
  if(s.seconds!=0)return false;
  for(uint32_t d:CAMERA_DENOMINATORS)if(d==s.denominator)return true;
  return false;
}
inline uint32_t cameraSpeedIndex(const CameraSpeedSettings& s) {
  if(s.denominator==0 && s.seconds>=1 && s.seconds<=30)return CAMERA_FRACTIONS+s.seconds-1;
  for(uint32_t i=0;i<CAMERA_FRACTIONS;i++)if(CAMERA_DENOMINATORS[i]==s.denominator)return i;
  return 19; // 1/100 s, same as a fresh CameraSpeedSettings
}
inline CameraSpeedSettings cameraSpeedAt(uint32_t index) {
  CameraSpeedSettings s;
  if(index>CAMERA_SPEED_MAX)index=CAMERA_SPEED_MAX;
  s.denominator=index<CAMERA_FRACTIONS?CAMERA_DENOMINATORS[index]:0;
  s.seconds=index<CAMERA_FRACTIONS?0:index-CAMERA_FRACTIONS+1;
  return s;
}
inline void formatCameraSpeed(uint32_t index,char* b,size_t n) {
  CameraSpeedSettings s=cameraSpeedAt(index);
  if(s.denominator)snprintf(b,n,"1/%lu s",(unsigned long)s.denominator);
  else snprintf(b,n,"%lu s",(unsigned long)s.seconds);
}
inline void formatBulbTime(uint32_t ms,char* b,size_t n) {
  if(ms>0 && ms<1000 && 1000%ms==0) {
    snprintf(b,n,"1/%lu s",(unsigned long)(1000/ms));return;
  }
  uint32_t fraction=ms%1000;
  if(!fraction){snprintf(b,n,"%lu s",(unsigned long)(ms/1000));return;}
  // Keep a previously saved custom time exact; do not label 0.333 s as 1/3 s.
  char part[4];snprintf(part,sizeof(part),"%03lu",(unsigned long)fraction);
  for(int i=2;i>0 && part[i]=='0';i--)part[i]=0;
  snprintf(b,n,"%lu.%s s",(unsigned long)(ms/1000),part);
}

// These reciprocals are exact with the existing millisecond command resolution.
// Do not lower the 100 ms Bulb limit or silently round a 1/60 s exposure command.
constexpr uint32_t BULB_FRACTIONS_MS[]={100,125,200,250,500};
constexpr int64_t BULB_MAX_INDEX=86404; // five fractions, then 1..86400 seconds
inline uint32_t stepBulbTime(uint32_t ms,int delta) {
  if(!delta)return ms;
  int64_t lower=-1;
  bool exact=false;
  if(ms>=1000){lower=5+ms/1000-1;exact=ms%1000==0;}
  else for(int i=0;i<5;i++) {
    if(BULB_FRACTIONS_MS[i]<=ms)lower=i;
    if(BULB_FRACTIONS_MS[i]==ms)exact=true;
  }
  int64_t index=lower+delta+(!exact && delta<0?1:0);
  if(index<0)index=0;
  if(index>BULB_MAX_INDEX)index=BULB_MAX_INDEX;
  return index<5?BULB_FRACTIONS_MS[index]:(uint32_t)(index-4)*1000;
}
