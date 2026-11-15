#pragma once
#include <stddef.h>
#include "Arduino.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct Preferences {
 std::map<std::string,std::vector<unsigned char>> data;
 bool readonly=false;
 bool begin(const char*,bool ro){readonly=ro;return true;}
 size_t getBytesLength(const char* key){auto i=data.find(key);return i==data.end()?0:i->second.size();}
 size_t getBytes(const char* key,void* dst,size_t n){auto i=data.find(key);if(i==data.end() || i->second.size()>n)return 0;std::memcpy(dst,i->second.data(),i->second.size());return i->second.size();}
 size_t putBytes(const char* key,const void* src,size_t n){assert(!testInDisplayFrame);if(readonly)return 0;auto b=(const unsigned char*)src;data[key]=std::vector<unsigned char>(b,b+n);return n;}
};
