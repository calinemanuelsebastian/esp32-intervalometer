// Host integration tests include the real application; mocks replace hardware only.
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdlib>
#ifndef APP_SOURCE
#define APP_SOURCE "../App.cpp"
#endif
#include APP_SOURCE
void at(uint32_t t,int sw){testSetMillis(t);externalLevels[ENC_SW]=sw;pollButton(t);}
void press(uint32_t t){at(t,LOW);at(t+30,LOW);at(t+100,HIGH);at(t+130,HIGH);}
int main(int argc,char** argv) {
 assert(argc==2);std::string scenario=argv[1];
 std::fill(modes,modes+40,-1);std::fill(externalLevels,externalLevels+40,-1);
 if(scenario=="pullup")testClobberPullup=true;
 if(scenario=="timer_init_failure")testFailCreate=true;
 if(scenario=="timer_intr_handle_failure")testFailIntrHandle=true;
 if(scenario=="timer_iram_mark_failure")testFailIramMark=true;
 if(scenario=="brightness_invalid"){OledSettings bad;bad.contrast=999;prefs.putBytes("oled",&bad,sizeof(bad));}
 if(scenario=="capture_migration") {
   Settings old;old.exposure=137;old.dryRun=0;prefs.putBytes("settings",&old,sizeof(old));
   CaptureSettings invalid;invalid.mode=42;prefs.putBytes("capture",&invalid,sizeof(invalid));
   uint32_t oldPanel[]={0x52375031,3};prefs.putBytes("panel",oldPanel,sizeof(oldPanel));
   uint32_t oldTheme[]={0x52375431,1};prefs.putBytes("theme",oldTheme,sizeof(oldTheme));
 }
 if(scenario=="settings_migration" || scenario=="settings_reboot" || scenario=="settings_invalid_ui") {
   Settings old;old.exposure=137;old.frames=41;old.dryRun=0;
   prefs.putBytes("settings",&old,sizeof(old));
   if(scenario=="settings_invalid_ui"){UiSettings invalid;invalid.menuClicks=0;prefs.putBytes("ui",&invalid,sizeof(invalid));}
 }
 if(scenario=="oled_missing")testOledAddress=0;
 if(scenario=="oled_address_3d")testOledAddress=0x3D;
 if(scenario=="oled_bus_stuck")testWireStuck=true;
 if(scenario=="camera_speed_invalid") {CameraSpeedSettings bad;bad.denominator=61;prefs.putBytes("speed",&bad,sizeof(bad));}
 if(scenario=="custom_exposure_retained") {
   Settings old;old.exposure=10;prefs.putBytes("settings",&old,sizeof(old));
   CaptureSettings oldCapture;oldCapture.fractionMs=375;prefs.putBytes("capture",&oldCapture,sizeof(oldCapture));
 }
 if(scenario=="button_boot_held")externalLevels[ENC_SW]=LOW;
 if(scenario=="oled_startup_recovery")testOledAddress=0;
 if(scenario=="reset_report")testResetReason=ESP_RST_BROWNOUT;
 if(scenario=="display_no_framebuffer")testCanvasMissing=true;
 setup();
 if(scenario=="boot") {
   assert(!controlsLocked && !runButtonGesture);
   assert(modes[ENC_SW]==INPUT_PULLUP);assert(digitalRead(ENC_SW)==HIGH);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
   assert(Wire.sda==23 && Wire.scl==18 && Wire.frequency==400000 && Wire.timeout==10);
   assert(oled.healthy() && oled.address()==0x3C);
   for(int pin:{13,19,21,22})assert(modes[pin]==-1);
   assert(ignoredWrites==0);
 } else if(scenario=="display_no_framebuffer") {
   assert(!displayAvailable() && renderedFrames==0);
   assert(Serial.output.find("framebuffer allocation failed")!=std::string::npos);
   cfg.dryRun=0;page=Page::Carousel;card=0;click(millis());
   assert(!seq.active() && outputLevels[SHUTTER]==LOW);
   page=Page::Manual;manualChoice=0;at(millis()+100,LOW);at(millis()+10,LOW);
   assert(!manual.active && outputLevels[SHUTTER]==LOW);loop();
 } else if(scenario=="pullup") {
   assert(modes[ENC_SW]==INPUT_PULLUP);
 } else if(scenario=="button_redraw_click" || scenario=="button_redraw_ordering") {
   testSetMillis(1000);page=scenario=="button_redraw_click"?Page::Home:Page::Carousel;
   card=0;dirty=true;button.begin(false,millis());
   testWireHook=[](){externalLevels[ENC_SW]=millis()>=1001 && millis()<1041?LOW:HIGH;};
   bool turned=false;
   for(int i=0;i<180;i++) {
     externalLevels[ENC_SW]=millis()>=1001 && millis()<1041?LOW:HIGH;
     if(scenario=="button_redraw_ordering" && millis()>=1041 && !turned) {
       encoderSteps=2*ui.menuClicks;turned=true;
     }
     loop();
   }
   testWireHook=nullptr;
   assert(totalPresses==1 && totalClicks==1 && !seq.active());
   assert(page==(scenario=="button_redraw_click"?Page::Carousel:Page::Settings));
 } else if(scenario=="button_boot_held") {
   assert(!button.isArmed());
   assert(std::find(canvas.text.begin(),canvas.text.end(),"Release knob SW:0")!=canvas.text.end());
   for(int i=0;i<2200;i++)loop();
   assert(page==Page::Home && totalPresses==0 && totalClicks==0);
   externalLevels[ENC_SW]=HIGH;
   for(int i=0;i<100;i++)loop();
   assert(button.isArmed() && !button.busy() && page==Page::Home);
   press(millis()+100);assert(totalClicks==1 && page==Page::Carousel);
 } else if(scenario=="reset_report") {
   assert(Serial.output.find("Boot reset reason: 9 (BROWNOUT)")!=std::string::npos);
   assert(page==Page::Home && !seq.active());
 } else if(scenario=="button_serial_state") {
   assert(page==Page::Home);Serial.output.clear();Serial.input="p";pollSerial(millis());
   assert(Serial.output.find("RAW=1")!=std::string::npos);
   assert(Serial.output.find("BTN down=0 stableDown=0 armed=1 busy=0")!=std::string::npos);
   assert(page==Page::Home);
   button.begin(true,millis());Serial.output.clear();Serial.input="p";pollSerial(millis());
   assert(Serial.output.find("armed=0 busy=1")!=std::string::npos);
 } else if(scenario=="navigation") {
   press(1000);assert(page==Page::Carousel);
   rotate(2*ui.menuClicks,1200);press(1300);assert(page==Page::Settings);
   press(1500);assert(page==Page::Edit);
   rotate(1,1800);press(1900);assert(page==Page::Settings && cfg.exposure==91);
   press(2200);rotate(1,2400);held(2500);assert(page==Page::Settings && cfg.exposure==91);
   page=Page::EncoderTest;diagnosticClicks=0;diagnosticPresses=0;
   press(2700);assert(diagnosticClicks==1 && diagnosticPresses==1 && page==Page::EncoderTest);
 } else if(scenario=="ordering") {
   page=Page::Carousel;card=0;
   at(1000,LOW);at(1030,LOW);at(1100,HIGH);
   encoderSteps=2*ui.menuClicks;testSetMillis(1130);loop();
   assert(page==Page::Settings && !seq.active());
 } else if(scenario=="display") {
   page=Page::Test;dirty=false;lightOn=true;lastDraw=1000;testSetMillis(1250);
   int before=renderedFrames;loop();assert(renderedFrames==(uint32_t)before+1);
   assert(lastRenderedPage==Page::Test && lastDraw>=testNow-1);
   page=Page::BacklightTest;backlightTestStart=2000;
   testSetMillis(3100);loop();assert(!lightOn);
   rotate(1,3150);assert(!lightOn); // interaction must not disrupt the timed test
   testSetMillis(10000);loop();assert(page==Page::Diagnostics && lightOn);
 } else if(scenario=="sequence_ui") {
   cfg.delay=0;cfg.exposure=2;cfg.frames=2;cfg.dryRun=1;
   page=Page::Carousel;card=0;testSetMillis(1000);click(testNow);
   testSetMillis(1001);loop();assert(page==Page::Running);
   testSetMillis(1500);loop();assert(seq.shutter && !lightOn && outputLevels[SHUTTER]==LOW);
   at(1550,LOW);at(1580,LOW);assert(lightOn);at(1650,HIGH);at(1680,HIGH);
   assert(page==Page::RunMenu);
   runChoice=1;click(1800);assert(seq.pauseRequested);
   testSetMillis(3500);loop();assert(seq.phase==Phase::Paused && !seq.pauseRequested && lightOn);
   click(3600);runChoice=1;click(3700);assert(seq.phase==Phase::Gap);
   click(3800);runChoice=2;click(3900);assert(seq.phase==Phase::Stopped && !seq.shutter);
 } else if(scenario=="encoder") {
   encoderPrevious=3;encoderAccum=0;encoderSteps=0;
   for(int state:{2,0,1,3}){externalLevels[ENC_A]=state>>1;externalLevels[ENC_B]=state&1;encoderISR();}
   int forward=takeRotation();assert(abs(forward)==4/ENCODER_EDGES_PER_STEP);
   for(int state:{1,0,2,3}){externalLevels[ENC_A]=state>>1;externalLevels[ENC_B]=state&1;encoderISR();}
   assert(takeRotation()==-forward);
   // One edge and its contact bounce must cancel before reaching a detent.
   for(int state:{2,3}){externalLevels[ENC_A]=state>>1;externalLevels[ENC_B]=state&1;encoderISR();}
   assert(takeRotation()==0);
   externalLevels[ENC_A]=0;externalLevels[ENC_B]=0;encoderISR();assert(takeRotation()==0);
 } else if(scenario=="serial_guard") {
   cfg.delay=0;seq.start(cfg,1000);page=Page::Running;
   for(char c:std::string("detbriqpusc")){Serial.input=std::string(1,c);pollSerial(1010);assert(page==Page::Running);}
 } else if(scenario=="gpio_reapply") {
   pinMode(ENC_SW,INPUT);Serial.output.clear();Serial.input="p";pollSerial(1000);
   assert(Serial.output.find("PU=0")!=std::string::npos);
   assert(modes[ENC_SW]==INPUT); // p is read-only
   Serial.output.clear();Serial.input="u";pollSerial(1010);
   assert(modes[ENC_SW]==INPUT_PULLUP && Serial.output.find("PU=1")!=std::string::npos);
   assert(Serial.output.find("IN=1 OUT=0 PU=1 PD=0 RAW=1")!=std::string::npos);
 } else if(scenario=="uart_full") {
   Serial.output.clear();Serial.space=0;page=Page::Home;
   press(1000);assert(page==Page::Carousel && totalClicks==1);
   assert(Serial.output.empty());
   seq.plan.dryRun=0;seq.shutter=seq.focus=true;applyOutputs();
   assert(outputLevels[SHUTTER]==HIGH && Serial.output.empty());
   seq.stop();applyOutputs();assert(outputLevels[SHUTTER]==LOW);
 } else if(scenario=="all_pages") {
   for(int i=0;i<=(int)Page::DisplayInfo;i++){page=(Page)i;draw(millis());assert(lastRenderedPage==page);}
   Settings maximum;maximum.exposure=86400;maximum.gap=3600;maximum.frames=9999;
   maximum.delay=3600;maximum.lightTimeout=120;maximum.dryRun=0;cfg=maximum;
   for(setting=0;setting<SETTING_COUNT;setting++) {
     page=Page::Edit;editValue=settingValue(setting);draw(millis());rotate(10000,1000);
     assert(editValue<=maxs[setting]);rotate(-100000,2000);assert(editValue==mins[setting]);
   }
   setting=SETTING_COUNT;page=Page::Settings;click(3000);assert(page==Page::Carousel);
 } else if(scenario=="scroll_sensitivity") {
   page=Page::Carousel;card=0;
   rotate(1,1000);assert(card==0);rotate(1,1200);assert(card==1);
   rotate(1,1300);rotate(-1,1400);assert(card==1);rotate(-1,1500);assert(card==0);
   // A partial turn must not carry into the next menu.
   rotate(1,1600);card=2;click(1700);assert(page==Page::Settings);
   rotate(1,1800);assert(setting==0);rotate(1,1900);assert(setting==1);
   page=Page::Edit;setting=0;editValue=90000;
   rotate(1,2000);rotate(1,2010);assert(editValue==92000); // no acceleration
   setting=6;editValue=4;click(2100);assert(ui.menuClicks==4);
   setting=0;rotate(3,2200);assert(setting==0);rotate(1,2300);assert(setting==1);
 } else if(scenario=="manual_hold") {
   cfg.dryRun=0;page=Page::Carousel;card=1;
   press(1000);assert(page==Page::Manual && !manual.active && outputLevels[SHUTTER]==LOW);
   at(1500,LOW);at(1530,LOW);
   assert(manual.active && outputLevels[SHUTTER]==HIGH && outputLevels[FOCUS]==HIGH && !lightOn);
   at(4000,LOW);assert(page==Page::Manual && manual.active && !controlsLocked); // past long-hold threshold
   rotate(8,4100);assert(manualChoice==0 && manual.active && lightOn);
   Serial.input="q";pollSerial(4200);assert(page==Page::Manual && manual.active);
   at(6000,HIGH);assert(!manual.active && outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==HIGH);
   assert(manual.lastDuration==4470 && lightOn);
   at(6005,LOW);at(6010,HIGH);assert(!manual.active); // release bounce cannot reopen
   at(6040,HIGH);assert(!manual.gesture && page==Page::Manual);
   rotate(ui.menuClicks,6100);assert(manualChoice==1);
   press(6200);assert(page==Page::Carousel && outputLevels[SHUTTER]==LOW);
 } else if(scenario=="manual_dry_tap") {
   cfg.dryRun=1;page=Page::Manual;manualChoice=0;
   at(1000,LOW);at(1030,LOW);assert(manual.active && outputLevels[SHUTTER]==LOW);
   at(1100,HIGH);at(1130,HIGH);assert(!manual.active && !manual.gesture && manual.lastDuration==70);
   assert(page==Page::Manual); // release is not a second trigger or menu confirmation
   cfg.dryRun=0;at(2000,LOW);at(2030,LOW);assert(outputLevels[SHUTTER]==HIGH);
   at(2050,HIGH);assert(outputLevels[SHUTTER]==LOW);at(2080,HIGH);
 } else if(scenario=="manual_boot_and_exit") {
   page=Page::Manual;manualChoice=0;cfg.dryRun=0;
   button.begin(true,1000);at(5000,LOW);assert(!manual.active && outputLevels[SHUTTER]==LOW);
   at(5100,HIGH);at(5130,HIGH);assert(!manual.active);
   at(5200,LOW);at(5230,LOW);assert(manual.active);
   // Even an unexpected mode change must release camera outputs.
   page=Page::Home;testSetMillis(5240);loop();assert(!manual.active && outputLevels[SHUTTER]==LOW);
 } else if(scenario=="manual_loop_release") {
   page=Page::Manual;cfg.dryRun=0;
   at(1000,LOW);at(1030,LOW);assert(manual.active);
   externalLevels[ENC_SW]=HIGH;testSetMillis(1040);loop();
   assert(!manual.active && outputLevels[SHUTTER]==LOW && lightOn);
   assert(manual.lastDuration==10); // output release precedes release debounce
 } else if(scenario=="settings_migration" || scenario=="settings_invalid_ui") {
   assert(cfg.exposure==137 && cfg.frames==41 && cfg.dryRun==0 && ui.menuClicks==2);
 } else if(scenario=="settings_reboot") {
   assert(cfg.exposure==137 && ui.menuClicks==2);
   ui.menuClicks=3;saveSettings();assert(!saveFailed);
   cfg=Settings();ui=UiSettings();setup();
   assert(cfg.exposure==137 && cfg.frames==41 && cfg.dryRun==0 && ui.menuClicks==3);
 } else if(scenario=="timer_blocked_ui" || scenario=="timer_dry" || scenario=="timer_late_irq") {
   cfg.delay=0;cfg.exposure=10;cfg.frames=2;cfg.gap=3;cfg.dryRun=scenario=="timer_dry";
   testSetMillis(1000);seq.start(cfg,millis());applyOutputs();
   testSetMillis(1500);loop();assert(seq.phase==Phase::Exposing);
   assert(outputLevels[SHUTTER]==(cfg.dryRun?LOW:HIGH));
   if(scenario=="timer_late_irq")testTimerLatencyUs=137;
   testSetMillis(11495);page=Page::RunMenu;draw(millis()); // SPI transfer spans alarm
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
   assert(seq.shutter && seq.completed==0); // loop still has not consumed event
   applyOutputs();assert(outputLevels[SHUTTER]==LOW); // cannot reassert
   PulseReading r=pulse.last();
   assert(r.valid && r.measured && r.complete && r.dryRun==(bool)cfg.dryRun);
   assert(r.elapsedUs==10000000+testTimerLatencyUs);
   testSetMillis(12000);loop();
   assert(seq.phase==Phase::Gap && seq.completed==1 && seq.since==11500);
   assert(Serial.output.find("Trigger: set=10000000")!=std::string::npos);
   testSetMillis(14000);loop();assert(outputLevels[SHUTTER]==LOW);
   testSetMillis(14500);loop();assert(seq.phase==Phase::Exposing);
   testSetMillis(24600);loop();assert(seq.phase==Phase::Done && seq.completed==2);
 } else if(scenario=="timer_cache_safe" || scenario=="timer_cache_legacy_control" ||
           scenario=="timer_global_mask" || scenario=="timer_clock_after_start") {
   assert(testTimer.iram);
   cfg.delay=0;cfg.exposure=5;cfg.frames=1;cfg.dryRun=0;
   testSetMillis(1000);seq.start(cfg,millis());applyOutputs();testSetMillis(1500);loop();
   assert(outputLevels[SHUTTER]==HIGH);
   uint64_t end=testTimer.origin+testTimer.deadline;
   bool late=scenario=="timer_cache_legacy_control" || scenario=="timer_global_mask";
   if(scenario=="timer_cache_legacy_control")testTimer.iram=false; // old allocation policy
   if(scenario=="timer_global_mask")testTimerLatencyUs=400000; // IRAM cannot bypass masking
   if(scenario=="timer_clock_after_start")testFailRead=true; // callback uses the IRAM clock
   testCacheOffStartUs=end-1000;testCacheOffEndUs=end+400000;
   testAdvanceUs(end-testClockUs); // no loop/display/Serial servicing
   assert(outputLevels[SHUTTER]==(late?HIGH:LOW));
   assert(outputLevels[FOCUS]==(late?HIGH:LOW));
   testAdvanceUs(400000);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
   PulseReading r=pulse.last();
   assert(r.valid && r.measured && r.complete);
   assert(r.requestedUs==5000000 && r.elapsedUs==(late?5400000:5000000));
   assert(r.irqLateUs==(late?400000:0));
   loop();assert(seq.phase==Phase::Done && seq.completed==1);
   assert(timingStats.minUs==r.elapsedUs && timingStats.maxUs==r.elapsedUs);
 } else if(scenario=="timer_stop_stale") {
   cfg.delay=0;cfg.exposure=10;cfg.dryRun=0;
   testSetMillis(1000);seq.start(cfg,millis());applyOutputs();testSetMillis(1500);loop();
   uint64_t oldAlarm=testTimer.deadline;
   testSetMillis(2000);uint64_t stoppedAt=testClockUs-testTimer.origin;
   page=Page::RunMenu;runChoice=2;click(millis());assert(outputLevels[SHUTTER]==LOW && !testTimer.armed);
   PulseReading r=pulse.last();assert(r.valid && !r.complete && r.elapsedUs==stoppedAt-(oldAlarm-10000000));
   page=Page::Manual;manualChoice=0;at(2100,LOW);at(2130,LOW);
   assert(outputLevels[SHUTTER]==HIGH);
   gptimer_alarm_event_data_t stale{oldAlarm,oldAlarm};
   testTimer.callback(&testTimer,&stale,testTimer.context);
   assert(outputLevels[SHUTTER]==HIGH); // cancelled alarm cannot end a manual hold
   at(2200,HIGH);at(2230,HIGH);
   testSetMillis(3000);seq.start(cfg,millis());applyOutputs();testSetMillis(3500);loop();
   assert(outputLevels[SHUTTER]==HIGH);
   testTimer.callback(&testTimer,&stale,testTimer.context);
   assert(outputLevels[SHUTTER]==HIGH); // nor a new timed shot
   gptimer_alarm_event_data_t staleWithNewValue{testClockUs-testTimer.origin,testTimer.deadline};
   testTimer.callback(&testTimer,&staleWithNewValue,testTimer.context);
   assert(outputLevels[SHUTTER]==HIGH); // IDF may read the new alarm while handling an old IRQ
   testSetMillis(13600);loop();assert(seq.completed==1 && outputLevels[SHUTTER]==LOW);
 } else if(scenario=="timer_init_failure" || scenario=="timer_arm_failure" || scenario=="timer_read_failure" ||
           scenario=="timer_intr_handle_failure" || scenario=="timer_iram_mark_failure") {
   cfg.delay=0;cfg.exposure=1;cfg.dryRun=0;page=Page::Carousel;card=0;
   testSetMillis(1000);click(millis());
   if(scenario=="timer_arm_failure")testFailArm=true;
   if(scenario=="timer_read_failure")testFailRead=true;
   testSetMillis(1500);loop();
   assert(!seq.active() && !pulse.healthy() && outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
   page=Page::Manual;at(2000,LOW);at(2030,LOW);assert(!manual.active);
 } else if(scenario=="timer_maximum_wrap") {
   cfg.delay=0;cfg.exposure=86400;cfg.frames=1;cfg.dryRun=0;
   testSetMillis(UINT32_MAX-700);seq.start(cfg,millis());applyOutputs();
   testAdvanceUs(500000);loop();assert(outputLevels[SHUTTER]==HIGH);
   uint64_t deadline=testTimer.deadline+testTimer.origin;
   testAdvanceUs(deadline-testClockUs-1);assert(outputLevels[SHUTTER]==HIGH);
   testAdvanceUs(1);assert(outputLevels[SHUTTER]==LOW);
   loop();assert(seq.phase==Phase::Done && pulse.last().elapsedUs==86400000000ULL);
 } else if(scenario=="timer_late_loop" || scenario=="timer_late_start") {
   cfg.delay=0;cfg.exposure=10;cfg.frames=2;cfg.dryRun=0;
   testSetMillis(1000);seq.start(cfg,millis());
   if(scenario=="timer_late_start")testSetMillis(1200); // output wasn't applied yet
   applyOutputs();
   testSetMillis(1500);loop();
   if(scenario=="timer_late_start")assert(outputLevels[SHUTTER]==LOW);
   testSetMillis(1700);loop();assert(outputLevels[SHUTTER]==HIGH);
   testSetMillis(30000);assert(outputLevels[SHUTTER]==LOW);
   loop();assert(seq.phase==Phase::Gap && seq.completed==1 && !seq.shutter);
   testSetMillis(30500);loop();assert(seq.shutter); // preserve fresh focus lead, no catch-up burst
 } else if(scenario=="timer_no_flash") {
   cfg.delay=0;cfg.dryRun=0;
   testSetMillis(1000);seq.start(cfg,millis());applyOutputs();testSetMillis(1500);loop();
   saveSettings();assert(saveFailed && prefs.data.empty());
 } else if(scenario=="lock_controls") {
   cfg.delay=0;cfg.exposure=30;cfg.dryRun=0;
   testSetMillis(1000);page=Page::Carousel;card=0;click(millis());
   testSetMillis(1500);loop();uint64_t deadline=testTimer.deadline;
   at(4000,LOW);at(4005,HIGH);at(4010,LOW);at(4040,LOW); // press bounce
   at(6039,LOW);assert(!controlsLocked);
   at(6040,LOW);assert(controlsLocked && seq.active());
   at(9000,LOW);assert(controlsLocked); // no repeat while continuously held
   at(9030,HIGH);at(9060,HIGH);assert(page==Page::Running && controlsLocked);
   setBacklight(false);rotate(20,9100);
   assert(lightOn && page==Page::Running && runChoice==0);
   setBacklight(false);press(9200);
   assert(lightOn && controlsLocked && page==Page::Running && !seq.pauseRequested);
   assert(outputLevels[SHUTTER]==HIGH && testTimer.deadline==deadline);
   draw(millis());assert(std::find(canvas.text.begin(),canvas.text.end(),"LOCKED")!=canvas.text.end());
   at(10000,LOW);at(10030,LOW);at(12030,LOW);assert(!controlsLocked && seq.active());
   at(12100,HIGH);at(12130,HIGH);assert(page==Page::Running);
   assert(testTimer.deadline==deadline && outputLevels[SHUTTER]==HIGH);
 } else if(scenario=="lock_menu_stop") {
   cfg.delay=0;cfg.exposure=30;cfg.dryRun=0;
   testSetMillis(1000);page=Page::Carousel;card=0;click(millis());
   testSetMillis(1500);loop();press(1600);assert(page==Page::RunMenu);
   runChoice=2;at(1800,LOW);at(1830,LOW);at(3830,LOW);
   assert(controlsLocked && seq.active() && page==Page::Running);
   at(3900,HIGH);at(3930,HIGH);assert(seq.active() && runChoice==0);
   at(4000,LOW);at(4030,LOW);at(6030,LOW);
   at(6100,HIGH);at(6130,HIGH);assert(!controlsLocked && page==Page::Running);
   press(6200);rotate(2*ui.menuClicks,6400);assert(runChoice==2);
   press(6500);assert(seq.phase==Phase::Stopped && !controlsLocked);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW && !testTimer.armed);
 } else if(scenario=="lock_phases") {
   cfg.delay=5;cfg.exposure=3;cfg.frames=2;cfg.gap=2;cfg.dryRun=0;
   testSetMillis(1000);page=Page::Carousel;card=0;click(millis());held(1100);
   assert(controlsLocked && seq.phase==Phase::Delay);
   testSetMillis(5500);loop();testSetMillis(6000);loop();
   assert(controlsLocked && seq.phase==Phase::Exposing);
   testSetMillis(9000);loop();assert(controlsLocked && seq.phase==Phase::Gap);
   assert(seq.completed==1 && pulse.last().elapsedUs==3000000);
   testSetMillis(10500);loop();testSetMillis(11000);loop();
   assert(controlsLocked && seq.phase==Phase::Exposing);
   testAdvanceUs(testTimer.origin+testTimer.deadline-testClockUs);loop();
   assert(!controlsLocked && seq.phase==Phase::Done && seq.completed==2);
   assert(pulse.last().elapsedUs==3000000 && lightOn);
   press(14200);assert(page==Page::Home);
   page=Page::Carousel;card=0;click(14500);assert(seq.active() && !controlsLocked);
 } else if(scenario=="lock_pause") {
   cfg.delay=5;testSetMillis(1000);page=Page::Carousel;card=0;click(millis());
   click(1100);runChoice=1;click(1200);assert(seq.phase==Phase::Paused);
   held(1300);assert(controlsLocked);press(1400);rotate(4,1600);
   assert(controlsLocked && seq.phase==Phase::Paused && page==Page::Running);
   held(1700);click(1800);runChoice=1;click(1900);
   assert(!controlsLocked && seq.phase==Phase::Gap);
 } else if(scenario=="lock_gesture_short" || scenario=="lock_gesture_long" || scenario=="lock_gesture_debounce") {
   cfg.delay=0;cfg.exposure=1;cfg.frames=1;
   testSetMillis(1000);page=Page::Carousel;card=0;click(millis());
   testSetMillis(1500);loop();held(1600);assert(controlsLocked);
   if(scenario=="lock_gesture_debounce")at(2490,LOW);
   else {at(1800,LOW);at(1830,LOW);}
   testSetMillis(2500);loop();assert(seq.phase==Phase::Done && !controlsLocked);
   // Finish the same gesture after completion; it must not become Home or Back.
   if(scenario=="lock_gesture_long") {at(4500,LOW);at(4600,HIGH);at(4630,HIGH);}
   else {at(2540,LOW);at(2600,HIGH);at(2630,HIGH);}
   assert(page==Page::Running && !button.busy() && !runButtonGesture);
   press(4800);assert(page==Page::Home); // a fresh gesture works normally
 } else if(scenario=="lock_failure") {
   cfg.delay=0;cfg.exposure=1;cfg.dryRun=0;
   testSetMillis(1000);page=Page::Carousel;card=0;click(millis());held(1100);
   testFailArm=true;testSetMillis(1500);loop();
   assert(!controlsLocked && !seq.active() && !pulse.healthy());
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
 } else if(scenario=="fraction_editor") {
   cfg.exposure=1;capture.fractionMs=0;page=Page::Settings;setting=0;click(millis());
   assert(editValue==1000);rotate(-1,millis());assert(editValue==500);
   char label[32];formatValue(0,editValue,label,sizeof(label));assert(std::string(label)=="1/2 s");
   click(millis());assert(cfg.exposure==0 && capture.fractionMs==500 && commandDurationMs()==500);
   click(millis());rotate(-1,millis());assert(editValue==250);held(millis());assert(capture.fractionMs==500);
   click(millis());rotate(-3,millis());assert(editValue==125);click(millis());
   cfg=Settings();capture=CaptureSettings();setup();assert(commandDurationMs()==125);
   page=Page::Settings;setting=0;click(millis());rotate(-10000,millis());assert(editValue==100);
   click(millis());assert(commandDurationMs()==100);page=Page::Home;draw(millis());
   assert(std::find(canvas.text.begin(),canvas.text.end(),"1/10 s")!=canvas.text.end());
 } else if(scenario=="camera_speed_reminder") {
   cfg.exposure=10;capture.fractionMs=125;capture.mode=1;capture.triggerMs=80;
   cfg.delay=0;cfg.frames=1;cfg.dryRun=0;page=Page::Settings;setting=0;click(millis());
   rotate(2,millis());char label[32];formatValue(0,editValue,label,sizeof(label));assert(std::string(label)=="1/60 s");
   draw(millis());assert(std::find(canvas.text.begin(),canvas.text.end(),"Reminder: set camera")!=canvas.text.end());
   click(millis());assert(cameraSpeed.denominator==60 && cfg.exposure==10 && capture.fractionMs==125);
   assert(commandDurationMs()==80); // the reminder never controls the remote pulse
   cameraSpeed=CameraSpeedSettings();setup();assert(cameraSpeed.denominator==60);
   page=Page::Settings;setting=0;click(millis());rotate(1,millis());held(millis());assert(cameraSpeed.denominator==60);
   page=Page::Home;draw(millis());assert(std::find(canvas.text.begin(),canvas.text.end(),"Set camera: 1/60 s")!=canvas.text.end());
   page=Page::Carousel;card=0;testSetMillis(2000);click(millis());testSetMillis(2500);loop();
   assert(seq.cameraTimed && seq.triggerDurationMs==80);testSetMillis(2580);loop();
   assert(seq.phase==Phase::Done && pulse.last().elapsedUs==80000);
   capture.mode=0;assert(commandDurationMs()==10125);
 } else if(scenario=="camera_speed_invalid") {
   assert(cameraSpeed.denominator==100 && cameraSpeed.seconds==0);
   page=Page::Settings;setting=0;capture.mode=1;click(millis());
   rotate(-100000,millis());assert(editValue==0);rotate(100000,millis());assert(editValue==CAMERA_SPEED_MAX);
 } else if(scenario=="custom_exposure_retained") {
   assert(commandDurationMs()==10375);
   page=Page::Settings;setting=0;click(millis());assert(editValue==10375);
   char label[32];formatValue(0,editValue,label,sizeof(label));assert(std::string(label)=="10.375 s");
   click(millis());assert(commandDurationMs()==10375);
   capture.mode=1;page=Page::Settings;setting=0;click(millis());rotate(2,millis());click(millis());
   capture.mode=0;assert(commandDurationMs()==10375);saveSettings();cfg=Settings();capture=CaptureSettings();setup();
   assert(commandDurationMs()==10375 && cameraSpeed.denominator==60);
 } else if(scenario=="subsecond") {
   cfg.exposure=0;capture.fractionMs=375;cfg.delay=0;cfg.frames=1;cfg.dryRun=0;
   page=Page::Carousel;card=0;testSetMillis(1000);click(millis());
   testSetMillis(1500);loop();assert(seq.shutter && seq.triggerDurationMs==375);
   capture.fractionMs=900; // active sequence keeps its snapshot
   testSetMillis(1870);draw(millis()); // release occurs inside SPI rendering
   assert(outputLevels[SHUTTER]==LOW);loop();
   assert(seq.phase==Phase::Done && pulse.last().elapsedUs==375000);
   assert(timingStats.count==1 && timingStats.minUs==375000);
   cfg.exposure=0;capture.fractionMs=0;normalizeDuration();assert(commandDurationMs()==100);
   cfg.exposure=86400;capture.fractionMs=999;normalizeDuration();assert(commandDurationMs()==86400000);
   assert(capture.fractionMs==0);
 } else if(scenario=="timer_setup_retry") {
   testArmHook=[](){
     static int attempts=0;testArmDelayUs=(++attempts==1)?2000:0;
     assert(outputLevels[SHUTTER]==LOW && testCriticalDepth==0);
   };
   assert(pulse.start(1000000,false) && pulse.healthy());
   assert(outputLevels[SHUTTER]==HIGH);
   testAdvanceUs(testTimer.origin+testTimer.deadline-testClockUs);
   assert(outputLevels[SHUTTER]==LOW && pulse.last().elapsedUs==1000000);
 } else if(scenario=="timer_prepare_low" || scenario=="timer_setup_budget") {
   testArmHook=[](){assert(outputLevels[SHUTTER]==LOW && testCriticalDepth==0);};
   testArmDelayUs=scenario=="timer_prepare_low"?1000:2000;
   bool ok=pulse.start(5000000,false);
   assert(ok==(scenario=="timer_prepare_low"));
   if(ok) {
     assert(outputLevels[SHUTTER]==HIGH);
     testAdvanceUs(testTimer.origin+testTimer.deadline-testClockUs);
     assert(outputLevels[SHUTTER]==LOW && pulse.last().elapsedUs==4999000);
   } else assert(!pulse.healthy() && !testTimer.armed && outputLevels[SHUTTER]==LOW);
   assert(testCriticalDepth==0);
 } else if(scenario=="timer_lost_irq" || scenario=="timer_timeout_render") {
   cfg.delay=0;cfg.exposure=1;cfg.frames=2;cfg.dryRun=0;
   testSetMillis(1000);seq.start(cfg,millis());applyOutputs();testSetMillis(1500);loop();
   assert(outputLevels[SHUTTER]==HIGH);testDropAlarm=true;
   if(scenario=="timer_timeout_render") {
     testSetMillis(2515);uint32_t start=millis();draw(start);
     assert(millis()-start<10 && dirty && outputLevels[SHUTTER]==HIGH);
   } else testSetMillis(2520);
   loop();assert(!seq.active() && !pulse.healthy() && seq.completed==0);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW && lightOn);
   auto reading=pulse.last();assert(reading.timedOut && !reading.complete && reading.measured);
   assert(reading.elapsedUs>=1020000 && reading.elapsedUs<1023000);
   assert(Serial.output.find("TIMER RELEASE TIMEOUT")!=std::string::npos);
   gptimer_alarm_event_data_t stale{testTimer.deadline+20000,testTimer.deadline};
   testTimer.callback(&testTimer,&stale,testTimer.context);
   assert(pulse.last().timedOut && !pulse.start(1000000,false));
   page=Page::Manual;at(millis()+100,LOW);at(millis()+10,LOW);
   assert(!manual.active && outputLevels[SHUTTER]==LOW);
 } else if(scenario=="render_defers_save") {
   page=Page::Edit;setting=1;editValue=cfg.gap+1;
   at(1000,LOW);at(1030,LOW);at(1100,HIGH);testSetMillis(1129);
   auto oldGap=cfg.gap;auto before=renderedFrames;
   testInDisplayFrame=true;draw(millis());testInDisplayFrame=false;
   assert(page==Page::Edit && cfg.gap==oldGap && renderedFrames==before && dirty);
   loop();assert(page==Page::Settings && cfg.gap==oldGap+1);
   assert(prefs.getBytesLength("settings")==sizeof(Settings));
 } else if(scenario=="render_defers_rotation") {
   page=Page::Carousel;card=0;encoderSteps=2*ui.menuClicks;
   auto before=renderedFrames;testInDisplayFrame=true;draw(millis());testInDisplayFrame=false;
   assert(card==0 && encoderSteps!=0 && renderedFrames==before+1); // rotation cannot starve display
   loop();assert(card==2 && encoderSteps==0);
 } else if(scenario=="timer_arm_overdue") {
   cfg.delay=0;cfg.exposure=0;capture.fractionMs=100;cfg.dryRun=0;
   page=Page::Carousel;card=0;testSetMillis(1000);click(millis());
   testArmDelayUs=120000;testSetMillis(1500);loop();
   assert(!pulse.healthy() && !seq.active() && !testTimer.armed);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
 } else if(scenario=="camera_mode") {
   cfg.delay=0;cfg.frames=1;cfg.dryRun=0;cfg.exposure=90;
   capture.mode=1;capture.triggerMs=100;
   page=Page::Carousel;card=0;testSetMillis(1000);click(millis());
   testSetMillis(1500);loop();assert(seq.cameraTimed && seq.triggerDurationMs==100);
   assert(std::string(phaseName())=="TRIGGERING");
   testSetMillis(1600);loop();assert(seq.phase==Phase::Done && pulse.last().elapsedUs==100000);
   page=Page::Home;draw(millis());
   assert(std::find(canvas.text.begin(),canvas.text.end(),"Set camera: 1/100 s")!=canvas.text.end());
 } else if(scenario=="capture_migration") {
   assert(cfg.exposure==137 && cfg.dryRun==0 && capture.mode==0 && capture.fractionMs==0);
   assert(commandDurationMs()==137000 && displaySettings.contrast==96);
 } else if(scenario=="capture_reboot") {
   cfg.exposure=0;capture.fractionMs=125;capture.mode=1;capture.triggerMs=80;
   saveSettings();assert(!saveFailed);capture=CaptureSettings();cfg=Settings();setup();
   assert(cfg.exposure==0 && capture.fractionMs==125 && capture.mode==1 && capture.triggerMs==80);
   capture.mode=0;assert(commandDurationMs()==125);
 } else if(scenario=="manual_fast") {
   cfg.dryRun=0;page=Page::Carousel;card=1;testSetMillis(1000);click(millis());
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==HIGH);
   at(1100,LOW);at(1104,LOW);assert(!manual.active);
   at(1105,LOW);assert(manual.active && outputLevels[SHUTTER]==HIGH);
   at(1200,HIGH);assert(outputLevels[SHUTTER]==LOW && manual.lastDuration==95);
   at(1201,LOW);at(1207,LOW);assert(!manual.active); // release bounce cannot reopen
   at(1210,HIGH);at(1240,HIGH);assert(!manual.gesture);
   rotate(ui.menuClicks,1300);assert(manualChoice==1 && outputLevels[FOCUS]==LOW);
 } else if(scenario=="manual_i2c") {
   cfg.dryRun=0;page=Page::Manual;manualChoice=0;testSetMillis(1000);applyOutputs();
   testWireHook=[](){if(millis()>=1002)externalLevels[ENC_SW]=LOW;};
   draw(millis());testWireHook=nullptr;
   assert(totalPresses==0 && !manual.active); // draw only yields; loop handles the edge
   while(millis()<1010)loop();
   assert(totalPresses==1 && manual.active && outputLevels[SHUTTER]==HIGH);
   assert(millis()<1015); // transfer stops when manual shutter turns the OLED off
   rotate(1,1015);testSetMillis(1020); // wake the screen during the held shot
   testWireHook=[](){if(millis()>=1025)externalLevels[ENC_SW]=HIGH;};
   draw(millis());testWireHook=nullptr;
   assert(manual.active);loop(); // first observed open contact releases before further display IO
   assert(!manual.active && outputLevels[SHUTTER]==LOW && manual.lastDuration<25);
   assert(lightOn);at(1100,HIGH);assert(!manual.gesture && page==Page::Manual);
 } else if(scenario=="oled_transport") {
   // Packed row bits must be transposed to the controller's vertical page bytes.
   canvas.fillScreen(0);canvas.drawPixel(0,0,1);canvas.drawPixel(0,7,1);
   canvas.drawPixel(15,2,1);canvas.drawPixel(127,63,1);Wire.packets.clear();
   assert(oled.present(canvas.getBuffer()));
   assert(Wire.packets.size()==72);
   assert(Wire.packets[0].bytes==std::vector<uint8_t>({0,0xB0,0,0x10}));
   assert(Wire.packets[1].bytes[0]==0x40 && Wire.packets[1].bytes[1]==0x81);
   assert(Wire.packets[1].bytes[16]==0x04 && Wire.packets.back().bytes.back()==0x80);
   setBacklight(false);assert(Wire.packets.back().bytes==std::vector<uint8_t>({0,0xAE}));
   setBacklight(true);assert(Wire.packets.back().bytes==std::vector<uint8_t>({0,0xAF}));
   Serial.input="c";pollSerial(1000);assert(page==Page::DisplayInfo);
 } else if(scenario=="timing_series_stress") {
   cfg.delay=0;cfg.frames=20;cfg.exposure=10;cfg.gap=1;cfg.dryRun=0;
   page=Page::Carousel;card=0;testSetMillis(1000);click(millis());
   testSetMillis(1500);loop();
   for(int i=0;i<20;i++) {
     assert(seq.phase==Phase::Exposing);
     testTimerLatencyUs=(i==10?100000:i%2?137:0);
     uint64_t end=testTimer.origin+testTimer.deadline+testTimerLatencyUs;
     // A long IRQ delay is measured after foreground execution was also blocked.
     // A responsive loop instead trips the separate lost-IRQ safety tests.
     testAdvanceUs(end-testClockUs-(testTimerLatencyUs>20000?0:5000));page=Page::Running;draw(millis());
     assert(outputLevels[SHUTTER]==LOW);loop();
     assert(timingStats.count==(uint32_t)i+1);
     if(i<19) {
       uint32_t gapStart=seq.since;
       testSetMillis(gapStart+500);loop();testSetMillis(gapStart+1000);loop();
     }
   }
   assert(seq.phase==Phase::Done && timingStats.minUs==10000000 && timingStats.maxUs==10100000);
   assert(timingStats.maxIrqLateUs==100000 && timingStats.count==20);
   Serial.input="s";pollSerial(millis());assert(page==Page::TimingSeries);
   // No fictitious zero-error reading after a failed timestamp; keep a missing count.
   timingStats.add(10000000,0,0,false,false);assert(timingStats.missing==1 && timingStats.count==20);
 } else if(scenario=="serial_recovery") {
   cfg.delay=0;cfg.dryRun=0;cfg.exposure=30;
   page=Page::Carousel;card=0;testSetMillis(1000);click(millis());testSetMillis(1500);loop();held(1600);
   externalLevels[ENC_SW]=LOW;button.begin(true,1700);
   Serial.input="!";pollSerial(1700);
   assert(!seq.active() && !manual.active && !controlsLocked && page==Page::Home);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW && !testTimer.armed);
   at(4000,LOW);assert(page==Page::Home);at(4100,HIGH);at(4130,HIGH);
   press(4200);assert(page==Page::Carousel);
   Serial.input="v";pollSerial(4400);assert(inputTrace);
   Serial.output.clear();reportInputs(4500);assert(Serial.output.find("alive ")!=std::string::npos);
   page=Page::Manual;manualChoice=0;at(5000,LOW);at(5005,LOW);assert(manual.active);
   Serial.input="!";pollSerial(5010);assert(!manual.active && outputLevels[FOCUS]==LOW);
 } else if(scenario=="brightness_preview") {
   assert(displaySettings.contrast==96);page=Page::Settings;setting=7;click(1000);
   rotate(1,1100);Wire.packets.clear();draw(millis());
   assert(Wire.packets[0].bytes==std::vector<uint8_t>({0,0x81,97}) && displaySettings.contrast==96);
   held(1200);Wire.packets.clear();draw(millis());
   assert(Wire.packets[0].bytes==std::vector<uint8_t>({0,0x81,96}));
   click(1300);rotate(2,1400);click(1500);assert(displaySettings.contrast==98 && !saveFailed);
   displaySettings=OledSettings();setup();assert(displaySettings.contrast==98);
   for(int i=0;i<=(int)Page::DisplayInfo;i++){page=(Page)i;draw(millis());assert(lastRenderedPage==page);}
 } else if(scenario=="brightness_invalid") {
   assert(displaySettings.contrast==96);
 } else if(scenario=="timing_navigation") {
   page=Page::Diagnostics;diagnosticChoice=4;click(1000);assert(page==Page::Timing);
   draw(millis());assert(std::find(canvas.text.begin(),canvas.text.end(),"No timed pulse yet")!=canvas.text.end());
   click(1100);assert(page==Page::Diagnostics);
   diagnosticChoice=DIAGNOSTIC_COUNT-1;click(1200);assert(page==Page::Carousel);
   Serial.input="r";pollSerial(1300);assert(page==Page::Timing);
 } else if(scenario=="oled_address_3d") {
   assert(oled.healthy() && oled.address()==0x3D);
 } else if(scenario=="oled_startup_recovery" || scenario=="oled_transient_recovery" ||
           scenario=="oled_recovery_held" || scenario=="oled_recovery_gesture" ||
           scenario=="oled_retry_bounded") {
   if(scenario!="oled_startup_recovery") {
     cfg.dryRun=0;cfg.delay=0;cfg.exposure=30;
     testSetMillis(1000);seq.start(cfg,millis());applyOutputs();testSetMillis(1500);loop();
     assert(outputLevels[SHUTTER]==HIGH);
     testWireStuck=true;draw(millis());
   }
   loop();assert(!displayAvailable() && !seq.active() && outputLevels[SHUTTER]==LOW);
   uint32_t failedAt=millis();int attempts=Wire.transactions;
   testOledAddress=0x3C;testWireStuck=scenario=="oled_retry_bounded";
   if(scenario=="oled_recovery_held")externalLevels[ENC_SW]=LOW;
   if(scenario=="oled_recovery_gesture")
     testWireHook=[](){if(displayRecovering)externalLevels[ENC_SW]=LOW;};
   while(millis()-failedAt<900)loop();
   assert(!displayAvailable() && Wire.transactions==attempts); // bounded retry interval
   while(millis()-failedAt<3300)loop();
   if(scenario=="oled_retry_bounded") {
     assert(!displayAvailable() && displayRetryCount>=2 && displayRetryCount<=3);
     assert(!seq.active() && outputLevels[SHUTTER]==LOW);
   } else {
     if(scenario=="oled_recovery_held") {
       assert(displayRetryCount==0 && !displayAvailable());
       externalLevels[ENC_SW]=HIGH;
       uint32_t releasedAt=millis();while(millis()-releasedAt<200)loop();
     }
     testWireHook=nullptr;
     assert(displayAvailable() && lightOn && page==Page::Home && !seq.active());
     assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
     assert(displayRetryCount==1);
     if(scenario=="oled_recovery_gesture") {
       assert(!button.isArmed() && totalClicks==0);
       externalLevels[ENC_SW]=HIGH;
       for(int i=0;i<100;i++)loop();
     }
     press(millis()+100);assert(page==Page::Carousel && !seq.active());
   }
 } else if(scenario=="oled_missing" || scenario=="oled_bus_stuck") {
   assert(!oled.healthy() && outputLevels[SHUTTER]==LOW);
   uint32_t before=millis();int transactions=Wire.transactions;loop();
   assert(millis()-before<5 && Wire.transactions==transactions); // no immediate retry storm
   page=Page::Carousel;card=0;click(millis());assert(!seq.active());
   page=Page::Manual;manualChoice=0;at(millis()+100,LOW);at(millis()+5,LOW);assert(!manual.active);
   Serial.input="!";pollSerial(millis());testOledAddress=0x3C;testWireStuck=false;
   Serial.input="o";pollSerial(millis());assert(oled.healthy() && page==Page::Home);
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
 } else if(scenario=="oled_disconnect") {
   cfg.delay=0;cfg.dryRun=0;page=Page::Carousel;card=0;testSetMillis(1000);click(millis());
   testSetMillis(1500);loop();assert(seq.shutter);
   testWireStuck=true;draw(millis());assert(!oled.healthy() && !seq.active());
   assert(outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
   int attempts=Wire.transactions;loop();loop();assert(Wire.transactions==attempts);
 } else if(scenario=="oled_manual_disconnect") {
   cfg.dryRun=0;page=Page::Manual;at(1000,LOW);at(1005,LOW);assert(manual.active);
   testWireStuck=true;rotate(1,1100);loop();
   assert(!manual.active && !oled.healthy() && outputLevels[SHUTTER]==LOW && outputLevels[FOCUS]==LOW);
 } else if(scenario=="oled_sh1106") {
   OledDisplay alternate(true);Wire.packets.clear();assert(alternate.begin(96));
   bool pump=false;for(auto &p:Wire.packets)if(p.bytes==std::vector<uint8_t>({0,0xAD,0x8B,0x33}))pump=true;
   assert(pump);Wire.packets.clear();assert(alternate.present(canvas.getBuffer()));
   assert(Wire.packets[0].bytes==std::vector<uint8_t>({0,0xB0,2,0x10}));
 } else if(scenario=="oled_short_write") {
   testWireShortWrite=true;draw(millis());assert(!oled.healthy() && oled.error()==254);
   assert(outputLevels[SHUTTER]==LOW);
 } else if(scenario=="ui_render") {
   const char* dir=std::getenv("OLED_RENDER_DIR");assert(dir);
   auto save=[&](const std::string& name){
     draw(millis());std::ofstream out(std::string(dir)+"/"+name+".pgm",std::ios::binary);
     out<<"P5\n128 64\n255\n";
     for(int y=0;y<64;y++)for(int x=0;x<128;x++)out.put(static_cast<char>(canvas.buffer[y*16+x/8]&(0x80>>(x&7))?255:0));
   };
   editValue=settingValue(setting);
   for(int i=0;i<=(int)Page::DisplayInfo;i++){page=(Page)i;save("page-"+std::to_string(i));}
   cfg.exposure=0;capture.fractionMs=125;page=Page::Settings;setting=0;save("settings-fraction");
   page=Page::Home;save("home-fraction");page=Page::Edit;editValue=125;save("edit-fraction");
   capture.mode=1;cameraSpeed.denominator=60;page=Page::Home;save("home-camera-reminder");
   page=Page::Edit;editValue=cameraSpeedIndex(cameraSpeed);save("edit-camera-reminder");
   cameraSpeed.denominator=8000;page=Page::Settings;save("settings-camera-fastest");
   capture.mode=0;cfg.exposure=90;capture.fractionMs=0;
   page=Page::Settings;setting=7;save("settings-brightness");
   page=Page::Edit;editValue=255;save("edit-brightness");
   cfg.delay=0;cfg.exposure=10;seq.start(cfg,millis());testAdvanceUs(500000);seq.tick(millis());
   page=Page::Running;controlsLocked=true;save("locked");
 } else {assert(false);}
 std::cout<<"PASS: "<<scenario<<"\n";
}
