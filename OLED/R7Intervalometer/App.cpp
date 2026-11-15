#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <stdarg.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include "OledDisplay.h"
#include "Sequence.h"
#include "Input.h"
#include "Controls.h"
#include "PulseTimer.h"
#include "Capture.h"
#include "Exposure.h"

// ESP32-WROOM Dev Module; numbers are GPIO labels, not header positions.
constexpr int ENC_A=32, ENC_B=33, ENC_SW=27;
constexpr int SHUTTER=25, FOCUS=26;
// Display pins/controller are in DisplayConfig.h.
constexpr int ENCODER_DIRECTION = 1;            // -1 reverses rotation
constexpr int ENCODER_EDGES_PER_STEP = 2;        // EC11 15-pulse/30-detent; use 4 if one click moves twice
static_assert(ENCODER_EDGES_PER_STEP==2 || ENCODER_EDGES_PER_STEP==4,"Encoder edges must be 2 or 4");
static_assert(ENCODER_DIRECTION==1 || ENCODER_DIRECTION==-1,"Encoder direction must be 1 or -1");
constexpr bool INPUT_DEBUG = false; // Serial Monitor 115200: raw pins + accepted input events

OledDisplay oled;
GFXcanvas1 canvas(OLED_WIDTH,OLED_HEIGHT);
OledSettings displaySettings;
bool displayFaultReported=false,displayRecovering=false;
uint32_t lastDisplayRetry=0,displayRetryCount=0;
constexpr uint32_t DISPLAY_RETRY_MS=1000;
bool displayAvailable(){return oled.healthy() && canvas.getBuffer();}
Preferences prefs;
Settings cfg;
Sequence seq(true); // completion comes from the hardware timer, never the UI loop
PulseTimer pulse(SHUTTER,FOCUS);
bool timedRequested=false,focusLead=false;
CaptureSettings capture;
CameraSpeedSettings cameraSpeed;
TimingStats timingStats;
bool inputTrace=INPUT_DEBUG;
UiSettings ui;
ScrollDivider scroll;
ManualShutter manual;
int manualChoice=0;
bool storageOK=false, saveFailed=false;
// A sequence-only input lock; never latch a manually held shutter.
bool controlsLocked=false,runButtonGesture=false;

enum class Page { Home, Carousel, Settings, Edit, Running, RunMenu, Test, Diagnostics, EncoderTest, BacklightTest, SystemInfo, Manual, Timing, TimingSeries, DisplayInfo };
Page page=Page::Home;
int card=0, setting=0, runChoice=0;
uint32_t editValue=0;
constexpr int SETTING_COUNT=10;
const char* labels[]={"Exposure", "Gap", "Frames", "Start delay", "Wake time", "Dry run", "Menu clicks", "Brightness", "Shot mode", "Trigger pulse"};
const uint32_t mins[]={100,1,0,0,1,0,1,0,0,50};
const uint32_t maxs[]={86400000,3600,9999,3600,120,1,4,255,1,1000};
const char* cards[]={"START", "MANUAL SHUTTER", "SETTINGS", "DIAGNOSTICS", "BACK"};
constexpr int CARD_COUNT=sizeof(cards)/sizeof(cards[0]);
uint32_t lastActivity=0, lastDraw=0;
bool dirty=true, lightOn=true;
Phase lastPhase=Phase::Idle;
int diagnosticChoice=0;
const char* diagnosticLabels[]={"Encoder", "Display", "Screen sleep", "System info", "Timing", "Timing series", "OLED info", "Back"};
constexpr int DIAGNOSTIC_COUNT=sizeof(diagnosticLabels)/sizeof(diagnosticLabels[0]);
int64_t diagnosticSteps=0;
uint32_t diagnosticClicks=0, diagnosticPresses=0, backlightTestStart=0;
uint32_t totalPresses=0, totalClicks=0;
uint32_t renderedFrames=0, drawDurationUs=0;
Page lastRenderedPage=Page::Home;
bool animatedPage() {
  return page==Page::Running || page==Page::Test || page==Page::EncoderTest ||
         page==Page::BacklightTest || page==Page::SystemInfo || page==Page::Manual;
}

portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
volatile int encoderSteps=0;
volatile uint8_t encoderPrevious=0;
volatile int8_t encoderAccum=0;
DRAM_ATTR static const int8_t quad[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
void ARDUINO_ISR_ATTR encoderISR() {
  uint8_t n=(digitalRead(ENC_A)<<1)|digitalRead(ENC_B);
  portENTER_CRITICAL_ISR(&encoderMux);
  uint8_t old=encoderPrevious;
  if ((n^old)==3) encoderAccum=0; // invalid simultaneous transition
  else encoderAccum = encoderAccum + quad[(old<<2)|n];
  encoderPrevious=n;
  if (encoderAccum>=ENCODER_EDGES_PER_STEP) { encoderSteps = encoderSteps + 1; encoderAccum=0; }
  if (encoderAccum<=-ENCODER_EDGES_PER_STEP) { encoderSteps = encoderSteps - 1; encoderAccum=0; }
  portEXIT_CRITICAL_ISR(&encoderMux);
}
int takeRotation() {
  portENTER_CRITICAL(&encoderMux);
  int n=encoderSteps; encoderSteps=0;
  portEXIT_CRITICAL(&encoderMux);
  return n*ENCODER_DIRECTION;
}

// Skip a diagnostic line instead of waiting for UART space while servicing a shot.
bool logReady(const char* format,...) {
  char line[128];va_list args;va_start(args,format);
  int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
  if(n<=0 || n>=(int)sizeof(line) || Serial.availableForWrite()<n)return false;
  return Serial.write((const uint8_t*)line,n)==(size_t)n;
}
void reportButtonPin(const char* label) {
  gpio_io_config_t state={};
  if(gpio_get_io_config((gpio_num_t)ENC_SW,&state)!=ESP_OK) {
    logReady("GPIO27 configuration read failed\n");return;
  }
  logReady("GPIO27 %s: IN=%d OUT=%d PU=%d PD=%d RAW=%d\n",label,
    state.ie,state.oe,state.pu,state.pd,digitalRead(ENC_SW));
}

uint32_t& field(int index) {
  switch(index) {
    case 0:return cfg.exposure; case 1:return cfg.gap; case 2:return cfg.frames;
    case 3:return cfg.delay; case 4:return cfg.lightTimeout; case 5:return cfg.dryRun; case 6:return ui.menuClicks; case 7:return displaySettings.contrast;
    case 8:return capture.mode; default:return capture.triggerMs;
  }
}
uint32_t settingValue(int index) {
  if(index==0)return capture.mode?cameraSpeedIndex(cameraSpeed):bulbDurationMs(cfg.exposure,capture.fractionMs);
  return field(index);
}
void normalizeDuration() {
  if(cfg.exposure==0 && capture.fractionMs<100)capture.fractionMs=100;
  if(cfg.exposure==86400)capture.fractionMs=0;
}
uint32_t commandDurationMs() {
  return capture.mode?capture.triggerMs:bulbDurationMs(cfg.exposure,capture.fractionMs);
}
void saveSettings() {
  // The stock ESP32 core can defer timer interrupts during flash operations.
  if(seq.active() || manual.gesture) {saveFailed=true;return;}
  bool savedConfig=storageOK && prefs.putBytes("settings",&cfg,sizeof(cfg))==sizeof(cfg);
  bool savedUi=storageOK && prefs.putBytes("ui",&ui,sizeof(ui))==sizeof(ui);
  bool savedDisplay=storageOK && prefs.putBytes("oled",&displaySettings,sizeof(displaySettings))==sizeof(displaySettings);
  bool savedCapture=storageOK && prefs.putBytes("capture",&capture,sizeof(capture))==sizeof(capture);
  bool savedSpeed=storageOK && prefs.putBytes("speed",&cameraSpeed,sizeof(cameraSpeed))==sizeof(cameraSpeed);
  saveFailed=!savedSpeed || !savedConfig || !savedUi || !savedDisplay || !savedCapture;
  logReady("%s\n",saveFailed ? "Settings save FAILED; RAM value still applied" : "Settings saved");
}
void setBacklight(bool on) {
  lightOn=on;
  if(oled.healthy())oled.power(on); // no separate backlight pin on this OLED
}
void activity(uint32_t now) {
  lastActivity=now;
  if(page!=Page::BacklightTest)setBacklight(true); // timed test owns the backlight
  dirty=true;
}
void applyOutputs() {
  if(!seq.active() && controlsLocked) {controlsLocked=false;scroll.reset();dirty=true;}
  if(seq.shutter) {
    if(!timedRequested) {
      timedRequested=true;
      if(!pulse.start((uint64_t)seq.triggerDurationMs*1000ULL,seq.plan.dryRun!=0)) {
        seq.stop();controlsLocked=false;scroll.reset();pulse.untimed(false,false);dirty=true;
        logReady("TIMER ERROR: sequence stopped; camera released. Reboot before retry.\n");
      } else seq.since=millis();
    }
    // Never reassert GPIOs after the interrupt has closed a pulse, even when
    // the main loop has not yet consumed its completion event.
    focusLead=false;return;
  }
  timedRequested=false;
  bool expose=manual.active && !manual.dryRun;
  bool readyManual=page==Page::Manual && manualChoice==0 && !seq.active();
  bool focus=manual.active ? !manual.dryRun : readyManual ? !cfg.dryRun : seq.focus && !seq.plan.dryRun;
  pulse.untimed(expose,focus);
  if(seq.focus && !manual.active && !focusLead)seq.focusSince=millis();
  focusLead=seq.focus && !manual.active;
}
void serviceCompletion() {
  if(pulse.serviceDeadline()) {
    seq.stop();manual.cancel(millis());controlsLocked=false;runButtonGesture=false;
    applyOutputs();page=Page::Running;dirty=true;
    logReady("TIMER RELEASE TIMEOUT: outputs OFF; sequence stopped. Reboot before retry.\n");
  }
  uint32_t closedAt;
  if(!pulse.takeCompletion(closedAt))return;
  seq.finishExposure(closedAt);
  PulseReading r=pulse.last();
  timingStats.add(r.requestedUs,r.elapsedUs,r.irqLateUs,r.measured,r.dryRun);
  if(r.measured)logReady("Trigger: set=%llu us actual~=%llu us error=%lld us %s\n",
    (unsigned long long)r.requestedUs,(unsigned long long)r.elapsedUs,
    (long long)r.elapsedUs-(long long)r.requestedUs,r.dryRun?"DRY":"LIVE");
  else logReady("Trigger released; timing readback failed.\n");
  logReady("Series: n=%lu min=%llu max=%llu spread=%llu us irqLateMax=%llu us %s\n",
    (unsigned long)timingStats.count,(unsigned long long)timingStats.minUs,
    (unsigned long long)timingStats.maxUs,(unsigned long long)(timingStats.maxUs-timingStats.minUs),
    (unsigned long long)timingStats.maxIrqLateUs,timingStats.dryRun?"DRY":"LIVE");
}

int wrap(int v,int n) { v%=n;return v<0?v+n:v; }
void rotate(int delta,uint32_t now) {
  if (!delta) return;
  if(inputTrace)logReady("INPUT: rotate %d\n",delta);
  activity(now);
  diagnosticSteps+=delta;
  if(controlsLocked && seq.active()){scroll.reset();return;} // wake only
  if(page==Page::Manual && (manual.gesture || digitalRead(ENC_SW)==LOW))return;
  if(page==Page::Edit) {
    // Exposure uses fractional presets; other fields retain one unit per detent.
    if(setting==0 && !capture.mode){editValue=stepBulbTime(editValue,delta);return;}
    int64_t v=(int64_t)editValue+delta;
    if(setting==0) {
      if(v<0)v=0;
      if(v>CAMERA_SPEED_MAX)v=CAMERA_SPEED_MAX;
      editValue=(uint32_t)v;return;
    }
    if(v<mins[setting])v=mins[setting];
    if(v>maxs[setting])v=maxs[setting];
    editValue=(uint32_t)v;return;
  }
  if(page!=Page::Carousel && page!=Page::Settings && page!=Page::RunMenu &&
     page!=Page::Diagnostics && page!=Page::Manual){scroll.reset();return;}
  delta=scroll.consume(delta,ui.menuClicks);
  if(!delta)return;
  if(page==Page::Diagnostics)diagnosticChoice=wrap(diagnosticChoice+delta,DIAGNOSTIC_COUNT);
  else if(page==Page::Carousel)card=wrap(card+delta,CARD_COUNT);
  else if(page==Page::Settings)setting=wrap(setting+delta,SETTING_COUNT+1);
  else if(page==Page::RunMenu)runChoice=wrap(runChoice+delta,3);
  else if(page==Page::Manual){manualChoice=wrap(manualChoice+delta,2);applyOutputs();}
}

void click(uint32_t now) {
  // Release the camera before any potentially slow OLED wake transaction.
  if(page==Page::RunMenu && runChoice==2 && !(controlsLocked && seq.active())) {
    seq.stop();applyOutputs();page=Page::Running;scroll.reset();activity(now);return;
  }
  scroll.reset();activity(now);
  if(controlsLocked && seq.active())return; // wake only; long hold owns unlock
  switch(page) {
    case Page::Home: page=Page::Carousel;card=0;break;
    case Page::Carousel:
      if(card==0) {
        controlsLocked=false;
        timingStats.reset();
        if(pulse.healthy() && displayAvailable())seq.start(cfg,now,commandDurationMs(),capture.mode!=0);else seq.stop();
        applyOutputs();page=Page::Running;
      }
      else if(card==1) {page=Page::Manual;manualChoice=0;manual.cancel(now);manual.lastDuration=0;applyOutputs();}
      else if(card==2) {page=Page::Settings;setting=0;}
      else if(card==3) {page=Page::Diagnostics;diagnosticChoice=0;}
      else page=Page::Home;
      break;
    case Page::Settings:
      if(setting==SETTING_COUNT) page=Page::Carousel;
      else {editValue=settingValue(setting);page=Page::Edit;}
      break;
    case Page::Edit:
      if(settingValue(setting)!=editValue) {
        if(setting==0) {
          if(capture.mode)cameraSpeed=cameraSpeedAt(editValue);
          else {cfg.exposure=editValue/1000;capture.fractionMs=editValue%1000;}
        } else field(setting)=editValue;
        normalizeDuration();saveSettings();
      }
      page=Page::Settings;break;
    case Page::Diagnostics:
      if(diagnosticChoice==0) {page=Page::EncoderTest;diagnosticSteps=0;diagnosticClicks=0;diagnosticPresses=0;}
      else if(diagnosticChoice==1) page=Page::Test;
      else if(diagnosticChoice==2) {page=Page::BacklightTest;backlightTestStart=now;}
      else if(diagnosticChoice==3) page=Page::SystemInfo;
      else if(diagnosticChoice==4) page=Page::Timing;
      else if(diagnosticChoice==5) page=Page::TimingSeries;
      else if(diagnosticChoice==6) page=Page::DisplayInfo;
      else page=Page::Carousel;
      break;
    case Page::Manual:
      // Shooting starts on the debounced press, not the click-on-release event.
      if(manualChoice==1){manual.cancel(now);page=Page::Carousel;applyOutputs();}
      break;
    case Page::EncoderTest:diagnosticClicks++;break;
    case Page::DisplayInfo:page=Page::Diagnostics;break;
    case Page::Test:case Page::BacklightTest:case Page::SystemInfo:case Page::Timing:case Page::TimingSeries:
      page=Page::Diagnostics;break;
    case Page::Running:
      if(seq.active()) {page=Page::RunMenu;runChoice=0;}
      else page=Page::Home;
      break;
    case Page::RunMenu:
      if(runChoice==1) {
        if(seq.phase==Phase::Paused) seq.resume(now);
        else if(seq.pauseRequested) seq.pauseRequested=false;
        else seq.pause();
      } else if(runChoice==2) seq.stop();
      applyOutputs();page=Page::Running;break;
  }
}
void held(uint32_t now) {
  scroll.reset();activity(now);
  if(seq.active()) {
    controlsLocked=!controlsLocked;page=Page::Running;runChoice=0;
    logReady("Controls %s; sequence continues.\n",controlsLocked?"LOCKED":"UNLOCKED");
  }
  else if(page==Page::Test || page==Page::EncoderTest || page==Page::BacklightTest || page==Page::SystemInfo || page==Page::Timing || page==Page::TimingSeries || page==Page::DisplayInfo)
    page=Page::Diagnostics;
  else if(page==Page::Diagnostics) page=Page::Carousel;
  else if(page==Page::Manual) {manual.cancel(now);page=Page::Carousel;applyOutputs();}
  else if(page==Page::Edit) page=Page::Settings; // cancel unconfirmed edit
  else page=Page::Home;
}
DebouncedButton button;
void pollButton(uint32_t now) {
  bool down=digitalRead(ENC_SW)==LOW;
  if(down!=button.rawDown())dirty=true; // Home exposes raw switch state, even before a click
  if(manual.releaseIfOpen(down,now)){applyOutputs();activity(now);}
  // Remember the context at the first physical down sample, not 30 ms later.
  // A gesture that crosses sequence completion must not become Home/Back.
  if(!button.busy() && down)runButtonGesture=seq.active();
  ButtonEvents e=button.update(down,now,page==Page::Manual && manualChoice==0?5:30);
  if(e.released)dirty=true; // redraw the boot-held release/arming hint
  bool sequenceEndedDuringGesture=runButtonGesture && !seq.active();
  if(!button.busy())runButtonGesture=false;
  if(sequenceEndedDuringGesture)return;
  if(e.pressed) {
    totalPresses++;
    if(page==Page::EncoderTest)diagnosticPresses++;
    if(inputTrace)logReady("INPUT: button pressed\n");
    if(page==Page::Manual && manualChoice==0 && !seq.active() && pulse.healthy() && displayAvailable()) {
      scroll.reset();manual.press(now,cfg.dryRun!=0);applyOutputs();
      lastActivity=now-cfg.lightTimeout*1000U;setBacklight(false);dirty=true;
    } else activity(now);
  }
  if(page==Page::Manual && manual.gesture) {
    // A long manual Bulb hold must never become a Lock/Unlock or Back action.
    if(e.click)totalClicks++;
    if(e.released)manual.finishGesture();
    return;
  }
  if(e.hold) {
    if(inputTrace)logReady("INPUT: long hold -> lock/unlock or back\n");
    held(now);
  } else if(e.click) {
    totalClicks++;
    if(inputTrace)logReady("INPUT: click on release\n");
    click(now);
  }
}
// Read-only yield check. No menu, GPIO, flash, Serial or display writes here.
// Button/timer events yield a partial frame. Rotation stays queued until the
// main loop: continuous turning must not starve completion of every frame.
bool canContinueDisplay() {
  if(displayRecovering)return true;
  return !pulse.needsService() &&
    !button.needsService(digitalRead(ENC_SW)==LOW,millis(),page==Page::Manual && manualChoice==0?5:30);
}
void reportButtonState() {
  logReady("BTN down=%d stableDown=%d armed=%d busy=%d press=%lu click=%lu page=%d lock=%d\n",
    button.rawDown(),button.stableDown(),button.isArmed(),button.busy(),
    (unsigned long)totalPresses,(unsigned long)totalClicks,(int)page,controlsLocked);
}
void reportInputs(uint32_t now) {
  static uint32_t last=0;
  if(inputTrace && now-last>=1000) {
    // Counters preserve evidence of clicks shorter than this reporting interval.
    if(logReady("alive %lu SW=%d A=%d B=%d press=%lu click=%lu page=%d drawn=%d frames=%lu BL=%d lock=%d\n",
      (unsigned long)now,digitalRead(ENC_SW),digitalRead(ENC_A),digitalRead(ENC_B),
      (unsigned long)totalPresses,(unsigned long)totalClicks,(int)page,(int)lastRenderedPage,
      (unsigned long)renderedFrames,lightOn,controlsLocked)){last=now;reportButtonState();}
  }
}

// Serial diagnostics work even when the encoder cannot navigate; ! releases outputs.
void pollSerial(uint32_t now) {
  if(!Serial.available())return;
  char c=Serial.read();
  if(c=='\r' || c=='\n')return;
  if(c=='?') {
    logReady("d=menu e=encoder t=display b=sleep-test i=system r=timing s=series c=OLED-info o=retry-OLED q=home (idle)\n");
    logReady("p=GPIO27 u=reapply input (idle); v=input trace; !=STOP, release outputs and recover input\n");return;
  }
  if(c=='v'){inputTrace=!inputTrace;logReady("Input trace %s\n",inputTrace?"ON":"OFF");return;}
  if(c=='p'){reportButtonPin("readback");reportButtonState();return;}
  if(c=='!') {
    seq.stop();manual.cancel(now);controlsLocked=false;runButtonGesture=false;
    page=Page::Home;scroll.reset();takeRotation();applyOutputs();
    pinMode(ENC_A,INPUT_PULLUP);pinMode(ENC_B,INPUT_PULLUP);pinMode(ENC_SW,INPUT_PULLUP);
    portENTER_CRITICAL(&encoderMux);
    encoderAccum=0;encoderPrevious=(digitalRead(ENC_A)<<1)|digitalRead(ENC_B);
    portEXIT_CRITICAL(&encoderMux);
    button.begin(digitalRead(ENC_SW)==LOW,now);activity(now);
    logReady("STOP. Outputs released; controls reset. Release held button before using it.\n");return;
  }
  if(c!='d' && c!='e' && c!='t' && c!='b' && c!='i' && c!='r' && c!='q' && c!='p' && c!='u' && c!='s' && c!='c' && c!='o')return;
  if(seq.active() || manual.gesture || (page==Page::Manual && digitalRead(ENC_SW)==LOW)) {logReady("Release the button / stop the sequence before diagnostics.\n");return;}
  if(c=='u') {
    reportButtonPin("before reapply");
    pinMode(ENC_SW,INPUT_PULLUP);
    delay(1); // input settling, allowed only with no active/paused sequence
    button.begin(digitalRead(ENC_SW)==LOW,millis());
    reportButtonPin("after reapply");return;
  }
  scroll.reset();
  if(c=='d'){page=Page::Diagnostics;diagnosticChoice=0;}
  if(c=='e'){page=Page::EncoderTest;diagnosticSteps=0;diagnosticClicks=0;diagnosticPresses=0;}
  if(c=='t')page=Page::Test;
  if(c=='b'){page=Page::BacklightTest;backlightTestStart=now;}
  if(c=='i')page=Page::SystemInfo;
  if(c=='r')page=Page::Timing;
  if(c=='q')page=Page::Home;
  if(c=='s')page=Page::TimingSeries;
  if(c=='c')page=Page::DisplayInfo;
  if(c=='o') {
    oled.begin((uint8_t)displaySettings.contrast);displayFaultReported=false;page=Page::Home;
    logReady("OLED retry: %s, address=0x%02X error=%u\n",oled.healthy()?"ACK/init OK":"FAILED",oled.address(),oled.error());
  }
  applyOutputs();activity(now);
}

void handleDisplayFault() {
  if(displayAvailable())return;
  seq.stop();manual.cancel(millis());controlsLocked=false;runButtonGesture=false;
  page=Page::Home;applyOutputs();
  if(!displayFaultReported) {
    logReady("OLED offline (error %u). Outputs OFF. Auto retry after button release; Serial o retries too.\n",oled.error());
    displayFaultReported=true;lastDisplayRetry=millis();
    button.begin(digitalRead(ENC_SW)==LOW,millis());
  }
}
// Monochrome layout: clip text to the actual screen width, never scale an old TFT frame.
void textAt(int x,int y,const char* text,uint16_t color=1,uint8_t size=1) {
  if(x<0 || y<0 || y+8*size>OLED_HEIGHT)return;
  char clipped[22];int cap=(OLED_WIDTH-x)/(6*size);
  if(cap<=0)return;
  if(cap>21)cap=21;
  size_t n=strlen(text);if(n>(size_t)cap)n=cap;
  memcpy(clipped,text,n);clipped[n]=0;
  canvas.setTextColor(color);canvas.setTextSize(size);canvas.setCursor(x,y);canvas.print(clipped);
}
void center(int y,const char* text,uint8_t size=1) {
  if(strlen(text)*6*size>OLED_WIDTH)size=1;
  int chars=(int)strlen(text);if(chars>21)chars=21;
  textAt((OLED_WIDTH-chars*6*size)/2,y,text,1,size);
}
void header(const char* title) {
  textAt(1,0,title);canvas.drawFastHLine(0,10,128,1);
}
void menuRow(int y,bool selected,const char* label,const char* value=nullptr) {
  if(selected)canvas.fillRect(0,y-1,128,11,1);
  uint16_t ink=selected?0:1;
  textAt(1,y,selected?">":" ",ink);textAt(10,y,label,ink);
  if(value)textAt(127-(int)strlen(value)*6,y,value,ink);
}
void formatValue(int i,uint32_t v,char* b,size_t n) {
  if(i==0) {
    if(capture.mode)formatCameraSpeed(v,b,n);else formatBulbTime(v,b,n);
    return;
  }
  if(i==5)snprintf(b,n,"%s",v?"ON (no trigger)":"OFF (camera live)");
  else if(i==7)snprintf(b,n,"%lu / 255",(unsigned long)v);
  else if(i==8)snprintf(b,n,"%s",v?"Camera M/Tv":"Bulb hold");
  else if(i==9)snprintf(b,n,"%lu ms",(unsigned long)v);
  else if(i==6)snprintf(b,n,"%lu clicks/item",(unsigned long)v);
  else if(i==2){if(v)snprintf(b,n,"%lu",(unsigned long)v);else snprintf(b,n,"Unlimited");}
  else snprintf(b,n,"%lu s",(unsigned long)v);
}
const char* phaseName() {
  switch(seq.phase) {
    case Phase::Delay:return "START DELAY";case Phase::Exposing:return seq.cameraTimed?"TRIGGERING":"EXPOSING";
    case Phase::Gap:return "GAP";case Phase::Paused:return "PAUSED";
    case Phase::Done:return "COMPLETE";case Phase::Stopped:return "STOPPED";
    default:return "READY";
  }
}
void draw(uint32_t now) {
  if(!displayAvailable())return;
  uint32_t drawStart=micros();Page renderingPage=page;dirty=false;
  canvas.fillScreen(0);char b[48],v[24];
  switch(page) {
    case Page::Home:
      header(pulse.healthy()?"INTERVALOMETER":"TIMER ERROR: REBOOT");
      center(15,cfg.dryRun?"DRY RUN":"CAMERA LIVE");
      if(capture.mode) {formatCameraSpeed(cameraSpeedIndex(cameraSpeed),v,sizeof(v));snprintf(b,sizeof(b),"Set camera: %s",v);}
      else formatBulbTime(bulbDurationMs(cfg.exposure,capture.fractionMs),b,sizeof(b));
      center(28,b);snprintf(b,sizeof(b),"Gap %lu s",(unsigned long)cfg.gap);center(41,b);
      snprintf(b,sizeof(b),"%s SW:%d",button.isArmed()?"Press: menu":"Release knob",digitalRead(ENC_SW));
      center(56,b);break;
    case Page::Carousel:
      header("MENU");canvas.drawRect(2,17,124,28,1);center(27,cards[card]);
      for(int i=0;i<CARD_COUNT;i++) {
        if(i==card)canvas.fillCircle(44+i*10,56,2,1);
        else canvas.drawCircle(44+i*10,56,2,1);
      }
      break;
    case Page::Settings: {
      header(saveFailed?"SETTINGS: SAVE ERROR":"SETTINGS");
      int first=setting>2?setting-2:0;
      for(int row=0;row<3;row++) {
        int i=first+row;if(i>SETTING_COUNT)break;
        if(i<SETTING_COUNT) {
          if(i==0)formatValue(0,settingValue(0),v,sizeof(v));
          else if(i==5)snprintf(v,sizeof(v),"%s",cfg.dryRun?"ON":"OFF");
          else if(i==8)snprintf(v,sizeof(v),"%s",capture.mode?"Camera":"Bulb");
          else if(i==2 && !cfg.frames)snprintf(v,sizeof(v),"INF");
          else snprintf(v,sizeof(v),"%lu",(unsigned long)field(i));
        }
        menuRow(15+row*13,i==setting,i==SETTING_COUNT?"Back":labels[i],i<SETTING_COUNT?v:nullptr);
      }
      center(56,"Press: select");break;
    }
    case Page::Edit:
      header(labels[setting]);formatValue(setting,editValue,b,sizeof(b));center(20,b,2);
      center(39,setting==0?(capture.mode?"Reminder: set camera":"Camera in Bulb"):
        setting==8?(editValue?"Set speed on camera":"Camera in Bulb"):
        setting==9?"Camera trigger only":"Turn to change");
      center(56,"Click:save Hold:back");break;
    case Page::Running: {
      header(pulse.healthy()?phaseName():"TIMER ERROR");if(seq.plan.dryRun)textAt(109,0,"DRY");
      if(seq.phase==Phase::Exposing || seq.phase==Phase::Gap || seq.phase==Phase::Delay) {
        uint32_t left=seq.remaining(now),sec=(left+999)/1000;
        if(left<1000)snprintf(b,sizeof(b),"0.%03lu s",(unsigned long)left);
        else snprintf(b,sizeof(b),"%02lu:%02lu:%02lu",(unsigned long)(sec/3600),(unsigned long)(sec/60%60),(unsigned long)(sec%60));
        center(15,b,2);
      } else center(18,seq.phase==Phase::Done?(seq.cameraTimed?"All triggers sent":"All frames done"):"Shutter released");
      uint32_t frame=seq.completed+(seq.phase==Phase::Exposing?1:0);
      if(seq.plan.frames)snprintf(b,sizeof(b),"%s %lu/%lu",seq.cameraTimed?"Trig":"Frame",(unsigned long)frame,(unsigned long)seq.plan.frames);
      else snprintf(b,sizeof(b),"%s %lu/unlim",seq.cameraTimed?"Trig":"Frame",(unsigned long)frame);
      center(35,b);
      if(seq.active()) {
        center(45,seq.pauseRequested?(controlsLocked?"LOCKED / PAUSE WAIT":"OPEN / PAUSE WAIT"):controlsLocked?"LOCKED":"UNLOCKED");
        center(56,controlsLocked?"Hold 2s: unlock":"Click:menu Hold:lock");
      } else center(56,"Press: home");
      break;
    }
    case Page::RunMenu:
      header("SEQUENCE CONTROLS");
      for(int i=0;i<3;i++)menuRow(15+i*13,i==runChoice,i==0?"Return":i==2?"Stop now":seq.phase==Phase::Paused?"Resume":seq.pauseRequested?"Cancel pause":"Pause after frame");
      center(56,"Hold 2s: lock");break;
    case Page::Diagnostics: {
      header("DIAGNOSTICS");int first=diagnosticChoice>2?diagnosticChoice-2:0;
      for(int row=0;row<3;row++){int i=first+row;if(i>=DIAGNOSTIC_COUNT)break;menuRow(15+row*13,i==diagnosticChoice,diagnosticLabels[i]);}
      center(56,"Press: select");break;
    }
    case Page::EncoderTest:
      header("ENCODER TEST");
      snprintf(b,sizeof(b),"A:%d B:%d SW:%d",digitalRead(ENC_A),digitalRead(ENC_B),digitalRead(ENC_SW));textAt(1,14,b);
      snprintf(b,sizeof(b),"Steps:%lld",(long long)diagnosticSteps);textAt(1,25,b);
      snprintf(b,sizeof(b),"Down:%lu",(unsigned long)diagnosticPresses);textAt(1,36,b);
      snprintf(b,sizeof(b),"Clicks:%lu",(unsigned long)diagnosticClicks);textAt(1,46,b);
      center(56,"Hold 2s: back");break;
    case Page::Manual: {
      header(pulse.healthy()?"MANUAL SHUTTER":"TIMER ERROR");
      center(13,cfg.dryRun?"DRY RUN":manualChoice==0?"READY / HALF PRESS":"CAMERA LIVE");
      uint32_t elapsed=manual.active?now-manual.since:manual.lastDuration;
      snprintf(b,sizeof(b),"%s %lu.%lus",manual.active?"Hold":"Last",(unsigned long)(elapsed/1000),(unsigned long)((elapsed%1000)/100));
      center(25,b,2);menuRow(44,manualChoice==0,"Hold to shoot");menuRow(55,manualChoice==1,"Back");break;
    }
    case Page::BacklightTest:
      header("SCREEN SLEEP TEST");center(22,"1s ON / 1s OFF");center(37,"Returns after 8s");center(56,"Press: back");break;
    case Page::SystemInfo:
      header("SYSTEM / v1.12 OLED");
      snprintf(b,sizeof(b),"Uptime:%lus",(unsigned long)(now/1000));textAt(1,14,b);
      snprintf(b,sizeof(b),"Heap:%lu",(unsigned long)ESP.getFreeHeap());textAt(1,24,b);
      snprintf(b,sizeof(b),"Frames:%lu",(unsigned long)renderedFrames);textAt(1,34,b);
      snprintf(b,sizeof(b),"Draw:%lu us",(unsigned long)drawDurationUs);textAt(1,44,b);
      center(56,"Press: back");break;
    case Page::Timing: {
      header(pulse.healthy()?"TRIGGER TIMING":"TIMER ERROR");PulseReading r=pulse.last();
      if(!r.valid){center(22,"No timed pulse yet");center(36,"Run a sequence first");}
      else {
        snprintf(b,sizeof(b),"Set:%llu.%03llu s",(unsigned long long)(r.requestedUs/1000000),(unsigned long long)(r.requestedUs/1000%1000));textAt(1,14,b);
        if(r.measured)snprintf(b,sizeof(b),"Got:%llu.%03llu s",(unsigned long long)(r.elapsedUs/1000000),(unsigned long long)(r.elapsedUs/1000%1000));
        else snprintf(b,sizeof(b),"Readback unavailable");
        textAt(1,24,b);
        if(r.measured && r.complete)snprintf(b,sizeof(b),"Error:%+lld us",(long long)r.elapsedUs-(long long)r.requestedUs);
        else snprintf(b,sizeof(b),"%s",r.timedOut?"Release timeout":r.complete?"Released":"Stopped early");
        textAt(1,34,b);
        textAt(1,44,r.dryRun?"DRY / simulated":"LIVE / GPIO times");
      }
      center(56,"Press: back");break;
    }
    case Page::TimingSeries:
      header("TIMING SERIES / us");
      snprintf(b,sizeof(b),"N:%lu miss:%lu %s",(unsigned long)timingStats.count,(unsigned long)timingStats.missing,timingStats.dryRun?"DRY":"LIVE");textAt(1,14,b);
      snprintf(b,sizeof(b),"Min:%llu",(unsigned long long)timingStats.minUs);textAt(1,24,b);
      snprintf(b,sizeof(b),"Max:%llu",(unsigned long long)timingStats.maxUs);textAt(1,34,b);
      snprintf(b,sizeof(b),"Span:%llu",(unsigned long long)(timingStats.maxUs-timingStats.minUs));textAt(1,44,b);
      center(56,"GPIO only; click:back");break;
    case Page::DisplayInfo:
      header("OLED CONFIGURATION");
      snprintf(b,sizeof(b),"%s / 128x64",oled.controller());textAt(1,14,b);
      snprintf(b,sizeof(b),"Addr:0x%02X %lukHz",oled.address(),(unsigned long)(OLED_I2C_HZ/1000));textAt(1,24,b);
      snprintf(b,sizeof(b),"SDA:%d SCL:%d",OLED_SDA,OLED_SCL);textAt(1,34,b);
      snprintf(b,sizeof(b),"Brightness:%lu",(unsigned long)displaySettings.contrast);textAt(1,44,b);
      center(56,"Press: back");break;
    case Page::Test:
      textAt(2,2,"MONO DISPLAY TEST");canvas.drawFastHLine(1,11,126,1);
      snprintf(b,sizeof(b),"Frame:%lu",(unsigned long)renderedFrames);center(15,b);
      for(int x=4;x<124;x+=8)canvas.fillRect(x,28,4,8,1);
      canvas.fillRect(2+(now/50)%116,41,8,5,1);
      center(55,"Press: back");canvas.drawRect(0,0,128,64,1);break;
  }
  uint8_t contrast=(uint8_t)(page==Page::Edit && setting==7?editValue:displaySettings.contrast);
  if(!oled.contrast(contrast) || !oled.present(canvas.getBuffer(),canContinueDisplay)){handleDisplayFault();return;}
  if(oled.interrupted()) {drawDurationUs=micros()-drawStart;dirty=true;return;}
  drawDurationUs=micros()-drawStart;renderedFrames++;lastRenderedPage=renderingPage;lastDraw=millis();
}

// Retry only with outputs released and the switch stably open. An intermittent
// display error must not require a power cycle or resume an interrupted shot.
void serviceDisplayRecovery(uint32_t now) {
  if(displayAvailable() || !displayFaultReported || !canvas.getBuffer())return;
  if(now-lastDisplayRetry<DISPLAY_RETRY_MS || button.busy() || digitalRead(ENC_SW)==LOW)return;
  seq.stop();manual.cancel(now);controlsLocked=false;runButtonGesture=false;
  page=Page::Home;scroll.reset();takeRotation();applyOutputs();
  displayRecovering=true;displayRetryCount++;
  if(oled.begin((uint8_t)displaySettings.contrast)) {
    // Stay dark until a complete new Home frame has been sent.
    lightOn=false;draw(millis());
    if(displayAvailable())setBacklight(true);
  }
  // Ignore every gesture that happened while reinitializing the panel.
  pinMode(ENC_A,INPUT_PULLUP);pinMode(ENC_B,INPUT_PULLUP);pinMode(ENC_SW,INPUT_PULLUP);
  takeRotation();button.begin(true,millis());
  displayRecovering=false;lastDisplayRetry=millis();dirty=true;
  if(displayAvailable()) {
    displayFaultReported=false;lastActivity=millis();
    logReady("OLED recovered: Home restored, sequence stopped. Release knob before next press.\n");
  } else logReady("OLED retry %lu failed (error %u); camera outputs remain OFF.\n",(unsigned long)displayRetryCount,oled.error());
}

void setup() {
  controlsLocked=false;runButtonGesture=false;
  // First operations release the camera. Hardware 10k pull-downs cover boot/reset.
  // Arduino core 3.x rejects digitalWrite before GPIO ownership is configured.
  // Preload output latches with the low-level API before enabling either driver.
  gpio_set_level((gpio_num_t)SHUTTER,LOW);gpio_set_level((gpio_num_t)FOCUS,LOW);
  pinMode(SHUTTER,OUTPUT);pinMode(FOCUS,OUTPUT);
  digitalWrite(SHUTTER,LOW);digitalWrite(FOCUS,LOW);
  lightOn=false;
  Serial.begin(115200);
  esp_reset_reason_t reason=esp_reset_reason();
  Serial.printf("Boot reset reason: %d (%s)\n",(int)reason,
    reason==ESP_RST_POWERON?"POWER ON":reason==ESP_RST_BROWNOUT?"BROWNOUT":
    reason==ESP_RST_PANIC?"PANIC":reason==ESP_RST_SW?"SOFTWARE":
    (reason==ESP_RST_INT_WDT || reason==ESP_RST_TASK_WDT || reason==ESP_RST_WDT)?"WATCHDOG":"OTHER");
  pinMode(ENC_A,INPUT_PULLUP);pinMode(ENC_B,INPUT_PULLUP);pinMode(ENC_SW,INPUT_PULLUP);
  storageOK=prefs.begin("r7interval",false);
  if(storageOK && prefs.getBytesLength("settings")==sizeof(Settings)) {
    Settings stored;
    if(prefs.getBytes("settings",&stored,sizeof(stored))==sizeof(stored) && validSettings(stored))cfg=stored;
  }
  if(storageOK && prefs.getBytesLength("ui")==sizeof(UiSettings)) {
    UiSettings storedUi;
    if(prefs.getBytes("ui",&storedUi,sizeof(storedUi))==sizeof(storedUi) && validUiSettings(storedUi))ui=storedUi;
  }
  capture=CaptureSettings();displaySettings=OledSettings();
  if(storageOK && prefs.getBytesLength("capture")==sizeof(CaptureSettings)) {
    CaptureSettings stored;
    if(prefs.getBytes("capture",&stored,sizeof(stored))==sizeof(stored) && validCaptureSettings(stored))capture=stored;
  }
  if(storageOK && prefs.getBytesLength("oled")==sizeof(OledSettings)) {
    OledSettings stored;
    if(prefs.getBytes("oled",&stored,sizeof(stored))==sizeof(stored) && validOledSettings(stored))displaySettings=stored;
  }
  cameraSpeed=CameraSpeedSettings();
  if(storageOK && prefs.getBytesLength("speed")==sizeof(CameraSpeedSettings)) {
    CameraSpeedSettings stored;
    if(prefs.getBytes("speed",&stored,sizeof(stored))==sizeof(stored) && validCameraSpeed(stored))cameraSpeed=stored;
  }
  normalizeDuration();displayFaultReported=false;
  saveFailed=!storageOK;
  if(!pulse.begin())Serial.println("TIMER ERROR: camera outputs disabled; reboot before retry.");
  else Serial.println("Trigger release: cache-safe IRAM interrupt ready.");
  bool found=oled.begin((uint8_t)displaySettings.contrast);
  Serial.printf("OLED configured %s 128x64 SDA=%d SCL=%d: %s addr=0x%02X error=%u\n",
    oled.controller(),OLED_SDA,OLED_SCL,found?"ACK/init OK":"NOT READY",oled.address(),oled.error());
  if(!canvas.getBuffer())Serial.println("OLED framebuffer allocation failed; triggering disabled.");
  canvas.setTextWrap(false);
  // Establish inputs after all peripheral initialization, then allow settling.
  pinMode(ENC_A,INPUT_PULLUP);pinMode(ENC_B,INPUT_PULLUP);pinMode(ENC_SW,INPUT_PULLUP);
  delay(1);
  button.begin(digitalRead(ENC_SW)==LOW,millis());
  encoderPrevious=(digitalRead(ENC_A)<<1)|digitalRead(ENC_B);
  attachInterrupt(digitalPinToInterrupt(ENC_A),encoderISR,CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B),encoderISR,CHANGE);
  reportButtonPin("boot");reportButtonState();
  lastActivity=millis();draw(lastActivity);setBacklight(true);
  Serial.printf("Firmware 1.12 OLED. Ready. Dry run: %s. Hold 2s to lock/unlock a sequence; stop from its menu.\n",cfg.dryRun?"ON":"OFF");
}
void loop() {
  handleDisplayFault();
  uint32_t now=millis();
  if(manual.active && (page!=Page::Manual || seq.active()))manual.cancel(now);
  if(manual.releaseIfOpen(digitalRead(ENC_SW)==LOW,now)){applyOutputs();activity(now);}
  serviceCompletion();now=millis();
  seq.tick(now);applyOutputs(); // timer ISR has already released any completed pulse
  if(seq.phase!=lastPhase) {
    logReady("%lu ms: %s; completed=%lu\n",(unsigned long)now,phaseName(),(unsigned long)seq.completed);
    if(seq.phase==Phase::Exposing) {lastActivity=now-cfg.lightTimeout*1000U;setBacklight(false);}
    else {lastActivity=now;setBacklight(true);}
    if((seq.phase==Phase::Done || seq.phase==Phase::Stopped) && page==Page::RunMenu)page=Page::Running;
    lastPhase=seq.phase;dirty=true;
  }
  pollSerial(millis());
  rotate(takeRotation(),millis()); // apply queued selection before a confirming click
  pollButton(millis());
  now=millis();
  reportInputs(now);
  bool wanted=(!manual.active && seq.phase!=Phase::Exposing) || now-lastActivity<cfg.lightTimeout*1000U;
  if(page==Page::BacklightTest) {
    uint32_t elapsed=now-backlightTestStart;
    if(elapsed>=8000){page=Page::Diagnostics;activity(now);wanted=true;}
    else wanted=(elapsed/1000)%2==0;
  }
  if(wanted!=lightOn){setBacklight(wanted);if(wanted)dirty=true;}
  // Bound redraws even when input noise keeps dirty=true.
  if(redrawDue(lightOn,dirty,animatedPage(),now,lastDraw))draw(now);
  handleDisplayFault();
  serviceDisplayRecovery(millis());
  delay(1);
}
