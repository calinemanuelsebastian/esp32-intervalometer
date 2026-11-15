#include <cassert>
#include <iostream>
#include "../Controls.h"
int main(){
 UiSettings s;assert(validUiSettings(s));s.menuClicks=0;assert(!validUiSettings(s));s.menuClicks=5;assert(!validUiSettings(s));
 ScrollDivider d;
 assert(d.consume(1,2)==0);assert(d.consume(1,2)==1);
 assert(d.consume(-1,2)==0);assert(d.consume(-1,2)==-1);
 assert(d.consume(1,3)==0);assert(d.consume(-1,3)==0);assert(d.consume(-2,3)==-1);
 d.reset();assert(d.consume(8,3)==2);assert(d.consume(1,3)==1);
 d.reset();assert(d.consume(-8,3)==-2);assert(d.consume(-1,3)==-1);
 d.reset();assert(d.consume(10,1)==10);
 ManualShutter m;m.press(UINT32_MAX-100,false);
 assert(m.active && !m.dryRun && !m.releaseIfOpen(true,20));
 assert(m.releaseIfOpen(false,49));assert(m.lastDuration==150 && !m.active && m.gesture);
 assert(!m.releaseIfOpen(false,100));m.finishGesture();assert(!m.gesture);
 m.press(1000,true);m.cancel(1400);assert(!m.active && !m.gesture && m.lastDuration==400);
 std::cout<<"PASS: scroll division, reversal, reset, settings bounds, manual release and rollover\n";
}
