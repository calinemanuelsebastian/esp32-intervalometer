#pragma once
struct SPIMock {int sck=-1,miso=-1,mosi=-1,ss=-1;void begin(int a,int b,int c,int d){sck=a;miso=b;mosi=c;ss=d;}};
inline SPIMock SPI;
