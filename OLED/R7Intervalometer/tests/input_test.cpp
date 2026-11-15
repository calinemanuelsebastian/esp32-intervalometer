#include <cassert>
#include <iostream>
#include "../Input.h"
// Original firmware logic retained only as a regression fixture.
struct OriginalButton {
  bool raw=false,stable=false,sent=false;
  uint32_t changed=0,pressed=0;
  ButtonEvents update(bool down,uint32_t now) {
    ButtonEvents e;
    if(down!=raw){raw=down;changed=now;}
    if(raw!=stable && now-changed>=30){
      stable=raw;
      if(stable){pressed=now;sent=false;}
      else if(!sent)e.click=true;
    }
    if(stable && !sent && now-pressed>=2000){sent=true;e.hold=true;}
    return e;
  }
};
int main() {
  OriginalButton old;
  old.update(true,100);old.update(true,130);old.update(false,2120);
  assert(old.update(false,2130).hold); // wrong: physical input is already released
  assert(!old.update(false,2150).click); // click was consumed

  DebouncedButton b;b.begin(false,0);
  assert(!b.update(true,100).pressed);
  assert(b.update(true,130).pressed);
  assert(!b.update(false,200).click);
  assert(b.update(false,230).click);
  assert(!b.update(false,240).click);
  // Release just under 2 seconds: old logic could emit HOLD during release debounce.
  b.begin(false,0);b.update(true,100);b.update(true,130);
  assert(!b.update(false,2120).hold);
  auto e=b.update(false,2130);assert(!e.hold && !e.click);
  e=b.update(false,2150);assert(e.click && !e.hold);
  // True hold fires once, with no click on subsequent release.
  b.begin(false,0);b.update(true,100);b.update(true,130);
  assert(b.update(true,2130).hold);assert(!b.update(true,2200).hold);
  b.update(false,2300);e=b.update(false,2330);assert(!e.hold && !e.click);
  // Contact bounce on both edges still produces one click.
  b.begin(false,0);b.update(true,100);b.update(false,104);b.update(true,108);
  assert(!b.update(true,137).pressed);assert(b.update(true,138).pressed);
  b.update(false,200);b.update(true,204);b.update(false,208);
  assert(!b.update(false,237).click);assert(b.update(false,238).click);
  // A held-at-boot switch produces no action; later presses still work.
  b.begin(true,0);assert(!b.update(true,3000).hold);
  b.update(false,3100);assert(!b.update(false,3130).click);
  b.update(true,3200);assert(b.update(true,3230).pressed);
  b.update(false,3300);assert(b.update(false,3330).click);
  // millis rollover.
  b.begin(false,UINT32_MAX-100);b.update(true,UINT32_MAX-50);
  assert(b.update(true,UINT32_MAX-20).pressed);
  b.update(false,20);assert(b.update(false,50).click);
  // Dirty redraw storms do not bypass the 40ms input service window.
  int draws=0;uint32_t last=0;
  for(uint32_t t=0;t<=1000;t++)if(redrawDue(true,true,false,t,last)){draws++;last=t;}
  assert(draws==25);assert(!redrawDue(false,true,true,5000,0));
  assert(!redrawDue(true,false,true,199,0));assert(redrawDue(true,false,true,200,0));
  std::cout<<"PASS: click/release, near-hold release regression, holds, bounce, boot-held, rollover, redraw cap\n";
}
