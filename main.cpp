#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <DHT.h>
#include <time.h>
#include "config.h"
#include "effects.h"
#include "matrix_content.h"

#if MQTT_ENABLED
#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
#endif

// ─── Objects ──────────────────────────────────────────────────────────────────
CRGB leds[LED_COUNT];
MD_Parola matrix = MD_Parola(MATRIX_HW_TYPE,
                              MATRIX_DIN, MATRIX_CLK, MATRIX_CS,
                              MATRIX_DEVICES);
MD_MAX72XX* mx = nullptr;
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(WEB_PORT);

// ─── LED state ────────────────────────────────────────────────────────────────
struct LedState {
  uint8_t effect     = 0;
  uint8_t brightness = 180;
  CRGB    color      = CRGB::Red;
  bool    on         = true;
} led;

const int NUM_EFFECTS = 20;

// ─── Matrix state ─────────────────────────────────────────────────────────────
MatrixMode    matMode     = MAT_TIME_TEMP;
char          customText[128] = "Hello!";
char          scrollBuf[256]  = "";
bool          newContent      = true;
uint8_t       currentQuote    = 0;
uint8_t       currentShape    = 0;
unsigned long lastCycle       = 0;
bool          autoCycle       = true;
bool          showingShape    = false;
unsigned long shapeStart      = 0;

// ─── Stopwatch ────────────────────────────────────────────────────────────────
struct Stopwatch {
  bool          running = false;
  unsigned long startMs = 0;
  unsigned long elapsed = 0;   // accumulated ms before current run
} sw;

unsigned long swElapsed() {
  return sw.running ? sw.elapsed + (millis() - sw.startMs) : sw.elapsed;
}

// ─── Countdown Timer ──────────────────────────────────────────────────────────
struct CountdownTimer {
  bool          running    = false;
  bool          finished   = false;
  unsigned long durationMs = 60000UL;
  unsigned long startMs    = 0;
  unsigned long pausedLeft = 0;
} ct;

unsigned long ctRemaining() {
  if (ct.finished) return 0;
  if (!ct.running) return ct.pausedLeft > 0 ? ct.pausedLeft : ct.durationMs;
  unsigned long elapsed = millis() - ct.startMs;
  if (elapsed >= ct.durationMs) return 0;
  return ct.durationMs - elapsed;
}

// ─── Format helpers ───────────────────────────────────────────────────────────
String fmtMS(unsigned long ms) {
  unsigned long t = ms / 1000;
  unsigned long h = t / 3600, m = (t % 3600) / 60, s = t % 60;
  unsigned long d = (ms % 1000) / 100;
  char buf[14];
  if (h > 0) snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu.%lu", h, m, s, d);
  else        snprintf(buf, sizeof(buf), "%02lu:%02lu.%lu", m, s, d);
  return String(buf);
}

String fmtCountdown(unsigned long ms) {
  unsigned long t = ms / 1000;
  unsigned long h = t / 3600, m = (t % 3600) / 60, s = t % 60;
  char buf[12];
  if (h > 0) snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
  else        snprintf(buf, sizeof(buf), "%02lu:%02lu", m, s);
  return String(buf);
}

// ─── Sensor / time ────────────────────────────────────────────────────────────
float temperature = NAN, humidity = NAN;
unsigned long lastDHTRead = 0;
bool timeReady = false;

void readDHT() {
  float t = USE_CELSIUS ? dht.readTemperature() : dht.readTemperature(true);
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { temperature=t; humidity=h; }
}

String getTimeString() {
  if (!timeReady) return String("--:--");
  struct tm ti;
  if (!getLocalTime(&ti)) return String("--:--");
  char buf[10]; strftime(buf, sizeof(buf), "%H:%M", &ti);
  return String(buf);
}

String buildTimeTempString() {
  String s = getTimeString();
  if (!isnan(temperature)) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), USE_CELSIUS ? "  %.1fC  %.0f%%" : "  %.1fF  %.0f%%",
             temperature, humidity);
    s += String(tmp);
  }
  return s;
}

// ─── Matrix helpers ───────────────────────────────────────────────────────────
void matrixScroll(const char* text) {
  strlcpy(scrollBuf, text, sizeof(scrollBuf));
  matrix.displayClear();
  matrix.displayScroll(scrollBuf, PA_LEFT, PA_SCROLL_LEFT, MATRIX_SPEED);
  showingShape = false;
}

void matrixStatic(const char* text) {
  // Only update if text changed to avoid flicker
  static char last[64] = "";
  if (strcmp(last, text) == 0) return;
  strlcpy(last, text, sizeof(last));
  strlcpy(scrollBuf, text, sizeof(scrollBuf));
  matrix.displayClear();
  matrix.print(scrollBuf);
  showingShape = false;
}

// ─── LED alert flash ──────────────────────────────────────────────────────────
static bool    alertActive  = false;
static uint8_t alertFlashes = 0;
static unsigned long alertLast = 0;
static CRGB    alertColor   = CRGB::Green;

void triggerAlert(CRGB color, uint8_t flashes = 6) {
  alertActive=true; alertFlashes=flashes; alertColor=color; alertLast=millis();
}

void handleAlert() {
  if (!alertActive) return;
  if (alertFlashes == 0) { alertActive=false; return; }
  if (millis()-alertLast >= 200) {
    alertLast=millis(); alertFlashes--;
    fill_solid(leds, LED_COUNT, (alertFlashes%2==0) ? alertColor : CRGB::Black);
    FastLED.show();
  }
}

// ─── LED effects ──────────────────────────────────────────────────────────────
void runEffect() {
  if (!led.on) { fill_solid(leds, LED_COUNT, CRGB::Black); return; }
  FastLED.setBrightness(led.brightness);
  switch (led.effect) {
    case 0:  fxSolid(led.color);         break;
    case 1:  fxBreathing(led.color);     break;
    case 2:  fxColorCycle();             break;
    case 3:  fxRainbow();                break;
    case 4:  fxRainbowCycle();           break;
    case 5:  fxColorWipe(led.color);     break;
    case 6:  fxTheaterChase(led.color);  break;
    case 7:  fxRunningLights(led.color); break;
    case 8:  fxLarson(led.color);        break;
    case 9:  fxTwinkle(led.color);       break;
    case 10: fxSparkle(led.color);       break;
    case 11: fxFire();                   break;
    case 12: fxComet(led.color);         break;
    case 13: fxStrobe(led.color);        break;
    case 14: fxSunrise();                break;
    case 15: fxMeteor(led.color);        break;
    case 16: fxConfetti();               break;
    case 17: fxPolice();                 break;
    case 18: fxCandle();                 break;
    case 19: fxWipeRandom();             break;
    default: fxSolid(led.color);         break;
  }
  FastLED.show();
}

// ─── Matrix update ────────────────────────────────────────────────────────────
static unsigned long lastMatTick = 0;

void updateMatrix() {
  unsigned long now = millis();

  // Stable clock mode — static HH:MM updating every second, blinking colon
  if (matMode == MAT_CLOCK) {
    if (now - lastMatTick >= 500) {   // update every 500ms for blinking colon
      lastMatTick = now;
      if (timeReady) {
        struct tm ti;
        if (getLocalTime(&ti)) {
          char buf[12];
          // Blink colon: odd 500ms = colon, even = space
          bool colonOn = (millis() / 500) % 2 == 0;
          strftime(buf, sizeof(buf), colonOn ? "%H:%M" : "%H %M", &ti);
          matrixStatic(buf);
        }
      } else {
        matrixStatic("--:--");
      }
    }
    matrix.displayAnimate();
    return;
  }


    if (now - lastMatTick >= 100) {
      lastMatTick = now;
      matrixStatic(fmtMS(swElapsed()).c_str());
    }
    matrix.displayAnimate();
    return;
  }

  // Timer mode — live display updating every 200ms
  if (matMode == MAT_TIMER) {
    if (now - lastMatTick >= 200) {
      lastMatTick = now;
      unsigned long rem = ctRemaining();
      if (ct.finished) matrixStatic("00:00");
      else             matrixStatic(fmtCountdown(rem).c_str());
    }
    matrix.displayAnimate();
    return;
  }

  // Shape: static for 4 seconds
  if (showingShape) {
    if (now - shapeStart >= 4000) {
      showingShape = false; newContent = true;
      if (autoCycle) { matMode=(MatrixMode)((matMode+1)%MAT_MODE_COUNT); lastCycle=now; }
    }
    return;
  }

  // Auto-cycle (skip timer/stopwatch modes)
  if (autoCycle && (now-lastCycle >= CYCLE_TIME_MS)) {
    do { matMode=(MatrixMode)((matMode+1)%MAT_MODE_COUNT); }
    while (matMode==MAT_STOPWATCH || matMode==MAT_TIMER || matMode==MAT_CLOCK);
    lastCycle=now; newContent=true;
    if (matMode==MAT_QUOTE) currentQuote=(currentQuote+1)%NUM_QUOTES;
    if (matMode==MAT_SHAPE) currentShape=(currentShape+1)%NUM_SHAPES;
  }

  if (newContent) {
    newContent = false;
    if (matMode==MAT_SHAPE) {
      if (mx) showShape(mx, SHAPES[currentShape]);
      showingShape=true; shapeStart=now; return;
    }
    switch (matMode) {
      case MAT_TIME_TEMP: matrixScroll(buildTimeTempString().c_str()); break;
      case MAT_QUOTE:     matrixScroll(QUOTES[currentQuote]);           break;
      case MAT_CUSTOM:    matrixScroll(customText);                     break;
      default:            matrixScroll(buildTimeTempString().c_str());  break;
    }
  }

  if (!showingShape && matrix.displayAnimate()) {
    if (matMode==MAT_TIME_TEMP) matrixScroll(buildTimeTempString().c_str());
    else matrix.displayReset();
  }
}

// ─── Web UI ───────────────────────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Control</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=DM+Mono:wght@400;500&family=Syne:wght@400;700&display=swap');
  *{box-sizing:border-box;margin:0;padding:0}
  :root{--bg:#0d0d0d;--card:#161616;--border:#2a2a2a;--accent:#ff4d00;--green:#22c55e;--blue:#3b82f6;--text:#f0ede8;--muted:#777;--radius:12px}
  body{background:var(--bg);color:var(--text);font-family:'Syne',sans-serif;min-height:100vh;padding:20px;max-width:520px;margin:0 auto}
  h1{font-size:1.6rem;font-weight:700;letter-spacing:-.02em;margin-bottom:2px}
  .sub{color:var(--muted);font-family:'DM Mono',monospace;font-size:.75rem;margin-bottom:24px}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}
  .card{background:var(--card);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:12px}
  .card h2{font-size:.65rem;text-transform:uppercase;letter-spacing:.12em;color:var(--muted);font-family:'DM Mono',monospace;margin-bottom:14px}
  .big{font-size:2.2rem;font-weight:700;letter-spacing:-.03em;line-height:1}
  .unit{font-size:.9rem;color:var(--muted);margin-left:3px}
  .clock{font-size:2.4rem;font-family:'DM Mono',monospace;font-weight:500;color:var(--accent);letter-spacing:.05em}
  .td{font-size:2.8rem;font-family:'DM Mono',monospace;font-weight:500;letter-spacing:.06em;text-align:center;padding:10px 0 6px;line-height:1;transition:color .3s}
  .td.green{color:var(--green)} .td.blue{color:var(--blue)} .td.red{color:#ef4444}
  label{display:block;font-size:.72rem;color:var(--muted);font-family:'DM Mono',monospace;margin-bottom:6px;margin-top:14px}
  label:first-of-type{margin-top:0}
  input[type=range]{width:100%;accent-color:var(--accent);cursor:pointer}
  input[type=color]{width:48px;height:32px;border:1px solid var(--border);border-radius:6px;cursor:pointer;background:none;padding:2px}
  input[type=text]{width:100%;background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--text);font-family:'DM Mono',monospace;font-size:.85rem;padding:10px 12px;outline:none}
  input[type=text]:focus{border-color:var(--accent)}
  input[type=number]{width:60px;background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--text);font-family:'DM Mono',monospace;font-size:.9rem;padding:8px;outline:none;text-align:center}
  input[type=number]:focus{border-color:var(--accent)}
  .row{display:flex;align-items:center;gap:10px}
  .tog{position:relative;width:44px;height:24px;flex-shrink:0}
  .tog input{opacity:0;width:0;height:0;position:absolute}
  .sl{position:absolute;inset:0;background:#333;border-radius:24px;cursor:pointer;transition:.2s}
  .tog input:checked+.sl{background:var(--accent)}
  .sl:before{content:'';position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
  .tog input:checked+.sl:before{transform:translateX(20px)}
  .btn{background:var(--accent);color:#fff;border:none;border-radius:8px;padding:10px 18px;font-family:'Syne',sans-serif;font-size:.85rem;font-weight:700;cursor:pointer;white-space:nowrap;transition:opacity .15s}
  .btn:hover{opacity:.85}
  .g{background:var(--green)} .bl{background:var(--blue)} .re{background:#ef4444}
  .gh{background:transparent;border:1px solid var(--border);color:var(--muted)}
  .gh:hover{border-color:var(--text);color:var(--text);opacity:1}
  .sm{padding:8px 14px;font-size:.78rem}
  .br{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
  .fx-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:6px}
  .fx-btn{background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--muted);font-family:'DM Mono',monospace;font-size:.7rem;padding:8px 6px;cursor:pointer;text-align:center;transition:all .15s}
  .fx-btn:hover{border-color:var(--accent);color:var(--text)}
  .fx-btn.active{border-color:var(--accent);background:rgba(255,77,0,.12);color:var(--accent)}
  .mm{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-bottom:14px}
  .mb{background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--muted);font-family:'DM Mono',monospace;font-size:.68rem;padding:9px 4px;cursor:pointer;text-align:center;transition:all .15s}
  .mb:hover{border-color:var(--accent);color:var(--text)}
  .mb.active{border-color:var(--accent);background:rgba(255,77,0,.12);color:var(--accent)}
  .dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--accent);margin-right:6px;animation:pulse 2s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
  .ip{font-family:'DM Mono',monospace;font-size:.72rem;color:var(--muted);margin-top:8px}
  .badge{display:inline-block;font-family:'DM Mono',monospace;font-size:.65rem;padding:3px 8px;border-radius:20px;margin-left:8px;vertical-align:middle}
  .b-run{background:rgba(34,197,94,.15);color:var(--green);border:1px solid rgba(34,197,94,.3)}
  .b-stop{background:rgba(119,119,119,.1);color:var(--muted);border:1px solid var(--border)}
  .b-done{background:rgba(239,68,68,.15);color:#ef4444;border:1px solid rgba(239,68,68,.3)}
  .laps{margin-top:10px;max-height:130px;overflow-y:auto}
  .lap{font-family:'DM Mono',monospace;font-size:.76rem;color:var(--muted);padding:4px 0;border-bottom:1px solid var(--border);display:flex;justify-content:space-between}
  .lap span{color:var(--text)}
  .ti{display:flex;align-items:center;gap:8px;margin-bottom:12px;flex-wrap:wrap}
  .ti label{margin:0}
</style>
</head>
<body>
<h1>LED Control</h1>
<p class="sub"><span class="dot"></span>ESP32 · WS2812B · MAX7219 · DHT22</p>

<div class="grid2">
  <div class="card"><h2>Temperature</h2><div class="big" id="temp">--<span class="unit">°C</span></div></div>
  <div class="card"><h2>Humidity</h2><div class="big" id="hum">--<span class="unit">%</span></div></div>
</div>
<div class="card"><h2>Time (IST)</h2><div class="clock" id="clock">--:--</div></div>

<div class="card">
  <h2>Stopwatch <span class="badge b-stop" id="sw-badge">stopped</span></h2>
  <div class="td green" id="sw-disp">00:00.0</div>
  <div class="br">
    <button class="btn g sm" id="sw-btn" onclick="swToggle()">Start</button>
    <button class="btn gh sm" onclick="swLap()">Lap</button>
    <button class="btn gh sm" onclick="swReset()">Reset</button>
    <button class="btn gh sm" onclick="setMatMode(4)">→ Matrix</button>
  </div>
  <div class="laps" id="lap-list"></div>
</div>

<div class="card">
  <h2>Timer <span class="badge b-stop" id="ct-badge">stopped</span></h2>
  <div class="td blue" id="ct-disp">01:00</div>
  <div class="ti">
    <label>H</label><input type="number" id="ct-h" value="0" min="0" max="23" onchange="ctSetDur()">
    <label>M</label><input type="number" id="ct-m" value="1" min="0" max="59" onchange="ctSetDur()">
    <label>S</label><input type="number" id="ct-s" value="0" min="0" max="59" onchange="ctSetDur()">
  </div>
  <div class="br">
    <button class="btn bl sm" id="ct-btn" onclick="ctToggle()">Start</button>
    <button class="btn gh sm" onclick="ctReset()">Reset</button>
    <button class="btn gh sm" onclick="setMatMode(5)">→ Matrix</button>
  </div>
</div>

<div class="card">
  <h2>LED Strip</h2>
  <div class="row" style="margin-bottom:14px">
    <span style="font-size:.8rem;flex:1">Power</span>
    <label class="tog"><input type="checkbox" id="ledOn" checked onchange="setPower(this.checked)"><span class="sl"></span></label>
  </div>
  <label>Brightness</label><input type="range" id="bri" min="0" max="255" value="180" oninput="setBri(this.value)">
  <label>Colour</label>
  <div class="row">
    <input type="color" id="cp" value="#ff0000" onchange="setColor(this.value)">
    <span style="font-family:'DM Mono',monospace;font-size:.75rem;color:var(--muted)" id="ch">#ff0000</span>
  </div>
  <label>Effect</label>
  <div class="fx-grid" id="fxGrid"></div>
</div>

<div class="card">
  <h2>Matrix Display</h2>
  <label style="margin-top:0">Mode</label>
  <div class="mm">
    <button class="mb active" id="mb0" onclick="setMatMode(0)">Clock+Temp</button>
    <button class="mb" id="mb1" onclick="setMatMode(1)">Quote</button>
    <button class="mb" id="mb2" onclick="setMatMode(2)">Shape</button>
    <button class="mb" id="mb3" onclick="setMatMode(3)">Custom</button>
    <button class="mb" id="mb4" onclick="setMatMode(4)">Stopwatch</button>
    <button class="mb" id="mb5" onclick="setMatMode(5)">Timer</button>
    <button class="mb" id="mb6" onclick="setMatMode(6)">Clock</button>
  </div>
  <div class="row" style="margin-bottom:14px">
    <span style="font-size:.8rem;flex:1">Auto-cycle</span>
    <label class="tog"><input type="checkbox" id="ac" checked onchange="setAC(this.checked)"><span class="sl"></span></label>
  </div>
  <label>Custom message</label>
  <div class="row" style="gap:8px">
    <input type="text" id="msg" placeholder="Type anything...">
    <button class="btn sm" onclick="sendText()">Send</button>
  </div>
  <label>Scroll speed</label><input type="range" id="mspd" min="10" max="120" value="40" oninput="setMSpd(this.value)">
  <label>Matrix brightness</label><input type="range" id="mbri" min="0" max="15" value="5" oninput="setMBri(this.value)">
</div>

<div class="ip" id="ipLabel"></div>

<script>
const FX=['Solid','Breathing','Color Cycle','Rainbow','Rainbow Cycle','Color Wipe','Theater Chase','Running Lights','Larson Scanner','Twinkle','Sparkle','Fire','Comet','Strobe','Sunrise','Meteor Rain','Confetti','Police','Candle','Wipe Random'];
let curFx=0,curMat=0;
const fg=document.getElementById('fxGrid');
FX.forEach((n,i)=>{const b=document.createElement('button');b.className='fx-btn'+(i===0?' active':'');b.textContent=n;b.id='fx'+i;b.onclick=()=>setFx(i);fg.appendChild(b);});

async function api(p,b){try{await fetch(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});}catch(e){}}
const pad=n=>String(n).padStart(2,'0');

// LED
function setPower(on){api('/api/power',{on});}
function setBri(v){api('/api/brightness',{brightness:+v});}
function setColor(h){document.getElementById('ch').textContent=h;api('/api/color',{r:parseInt(h.slice(1,3),16),g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16)});}
function setFx(i){curFx=i;document.querySelectorAll('.fx-btn').forEach((b,j)=>b.classList.toggle('active',i===j));api('/api/effect',{effect:i});}

// Matrix
function setMatMode(m){curMat=m;document.querySelectorAll('.mb').forEach((b,j)=>b.classList.toggle('active',m===j));api('/api/matrix/mode',{mode:m});}
function setAC(on){api('/api/matrix/autocycle',{auto:on});}
function sendText(){const t=document.getElementById('msg').value;if(!t)return;api('/api/matrix/text',{text:t});setMatMode(3);}
function setMSpd(v){api('/api/matrix/speed',{speed:+v});}
function setMBri(v){api('/api/matrix/brightness',{brightness:+v});}

// ── Stopwatch ─────────────────────────────────────────────────────────────
let swRun=false,swAcc=0,swT0=0,swTick=null,laps=[],lapN=0;
function swMs(){return swRun?swAcc+(Date.now()-swT0):swAcc;}
function fmtSw(ms){const t=Math.floor(ms/1000),h=Math.floor(t/3600),m=Math.floor((t%3600)/60),s=t%60,d=Math.floor((ms%1000)/100);return h>0?pad(h)+':'+pad(m)+':'+pad(s)+'.'+d:pad(m)+':'+pad(s)+'.'+d;}
function swToggle(){
  if(!swRun){
    swRun=true;swT0=Date.now();
    document.getElementById('sw-badge').className='badge b-run';document.getElementById('sw-badge').textContent='running';
    document.getElementById('sw-btn').textContent='Pause';document.getElementById('sw-btn').className='btn gh sm';
    swTick=setInterval(()=>document.getElementById('sw-disp').textContent=fmtSw(swMs()),100);
    api('/api/sw/start',{});
  } else {
    swAcc=swMs();swRun=false;clearInterval(swTick);
    document.getElementById('sw-badge').className='badge b-stop';document.getElementById('sw-badge').textContent='paused';
    document.getElementById('sw-btn').textContent='Resume';document.getElementById('sw-btn').className='btn g sm';
    api('/api/sw/pause',{elapsed:swAcc});
  }
}
function swLap(){if(!swRun)return;lapN++;laps.unshift({n:lapN,t:fmtSw(swMs())});if(laps.length>10)laps.pop();renderLaps();api('/api/sw/lap',{});}
function swReset(){clearInterval(swTick);swRun=false;swAcc=0;lapN=0;laps=[];document.getElementById('sw-disp').textContent='00:00.0';document.getElementById('sw-badge').className='badge b-stop';document.getElementById('sw-badge').textContent='stopped';document.getElementById('sw-btn').textContent='Start';document.getElementById('sw-btn').className='btn g sm';renderLaps();api('/api/sw/reset',{});}
function renderLaps(){document.getElementById('lap-list').innerHTML=laps.map(l=>`<div class="lap"><span style="color:var(--muted)">Lap ${l.n}</span><span>${l.t}</span></div>`).join('');}

// ── Countdown Timer ───────────────────────────────────────────────────────
let ctRun=false,ctDur=60000,ctRem=60000,ctT0=0,ctTick=null,ctDone=false;
function fmtCt(ms){const t=Math.max(0,Math.floor(ms/1000)),h=Math.floor(t/3600),m=Math.floor((t%3600)/60),s=t%60;return h>0?pad(h)+':'+pad(m)+':'+pad(s):pad(m)+':'+pad(s);}
function ctSetDur(){const h=+document.getElementById('ct-h').value||0,m=+document.getElementById('ct-m').value||0,s=+document.getElementById('ct-s').value||0;ctDur=Math.max(1000,(h*3600+m*60+s)*1000);ctRem=ctDur;ctDone=false;document.getElementById('ct-disp').textContent=fmtCt(ctRem);document.getElementById('ct-disp').className='td blue';api('/api/ct/set',{duration:Math.floor(ctDur/1000)});}
function ctToggle(){
  if(ctDone){ctReset();return;}
  if(!ctRun){
    ctRun=true;ctT0=Date.now();
    document.getElementById('ct-badge').className='badge b-run';document.getElementById('ct-badge').textContent='running';
    document.getElementById('ct-btn').textContent='Pause';document.getElementById('ct-btn').className='btn gh sm';
    ctTick=setInterval(()=>{
      const rem=Math.max(0,ctRem-(Date.now()-ctT0));
      document.getElementById('ct-disp').textContent=fmtCt(rem);
      if(rem<=10000)document.getElementById('ct-disp').className='td red';
      if(rem===0){clearInterval(ctTick);ctRun=false;ctDone=true;document.getElementById('ct-badge').className='badge b-done';document.getElementById('ct-badge').textContent='done!';document.getElementById('ct-btn').textContent='Reset';document.getElementById('ct-btn').className='btn re sm';}
    },200);
    api('/api/ct/start',{});
  } else {
    ctRem=Math.max(0,ctRem-(Date.now()-ctT0));ctRun=false;clearInterval(ctTick);
    document.getElementById('ct-badge').className='badge b-stop';document.getElementById('ct-badge').textContent='paused';
    document.getElementById('ct-btn').textContent='Resume';document.getElementById('ct-btn').className='btn bl sm';
    api('/api/ct/pause',{});
  }
}
function ctReset(){clearInterval(ctTick);ctRun=false;ctDone=false;ctRem=ctDur;document.getElementById('ct-disp').textContent=fmtCt(ctRem);document.getElementById('ct-disp').className='td blue';document.getElementById('ct-badge').className='badge b-stop';document.getElementById('ct-badge').textContent='stopped';document.getElementById('ct-btn').textContent='Start';document.getElementById('ct-btn').className='btn bl sm';api('/api/ct/reset',{});}

// ── Clock ─────────────────────────────────────────────────────────────────
function updateClock(){const n=new Date(),ist=new Date(n.getTime()+n.getTimezoneOffset()*60000+19800000);document.getElementById('clock').textContent=pad(ist.getHours())+':'+pad(ist.getMinutes());}
updateClock();setInterval(updateClock,10000);

// ── Poll ──────────────────────────────────────────────────────────────────
async function poll(){
  try{
    const d=await(await fetch('/api/status')).json();
    document.getElementById('temp').innerHTML=d.temperature?d.temperature.toFixed(1)+'<span class="unit">°'+(d.celsius?'C':'F')+'</span>':'--<span class="unit">°C</span>';
    document.getElementById('hum').innerHTML=d.humidity?Math.round(d.humidity)+'<span class="unit">%</span>':'--<span class="unit">%</span>';
    document.getElementById('ipLabel').textContent='IP: '+d.ip;
    document.getElementById('ledOn').checked=d.on;
    document.getElementById('bri').value=d.brightness;
    document.getElementById('ac').checked=d.autoCycle;
    if(d.effect!==curFx){curFx=d.effect;document.querySelectorAll('.fx-btn').forEach((b,j)=>b.classList.toggle('active',d.effect===j));}
    if(d.matMode!==curMat){curMat=d.matMode;document.querySelectorAll('.mb').forEach((b,j)=>b.classList.toggle('active',d.matMode===j));}
  }catch(e){}
}
poll();setInterval(poll,8000);
</script>
</body>
</html>)rawhtml");
}

// ─── API handlers ─────────────────────────────────────────────────────────────
void parseBody(JsonDocument& doc){ deserializeJson(doc, server.arg("plain")); }

void handleApiStatus(){
  JsonDocument doc;
  doc["temperature"]=isnan(temperature)?0.0f:temperature;
  doc["humidity"]=isnan(humidity)?0.0f:humidity;
  doc["celsius"]=USE_CELSIUS;
  doc["effect"]=led.effect; doc["brightness"]=led.brightness; doc["on"]=led.on;
  doc["matMode"]=(int)matMode; doc["autoCycle"]=autoCycle;
  doc["time"]=getTimeString(); doc["ip"]=WiFi.localIP().toString();
  doc["swRunning"]=sw.running; doc["swElapsed"]=(unsigned long)swElapsed();
  doc["ctRunning"]=ct.running; doc["ctFinished"]=ct.finished;
  doc["ctRemaining"]=(unsigned long)ctRemaining();
  String out; serializeJson(doc,out);
  server.send(200,"application/json",out);
}

void handlePower()     {JsonDocument d;parseBody(d);led.on=d["on"]|true;server.send(200,"application/json","{\"ok\":true}");}
void handleBrightness(){JsonDocument d;parseBody(d);led.brightness=constrain((int)(d["brightness"]|180),0,255);server.send(200,"application/json","{\"ok\":true}");}
void handleColor()     {JsonDocument d;parseBody(d);led.color=CRGB(d["r"]|255,d["g"]|0,d["b"]|0);server.send(200,"application/json","{\"ok\":true}");}
void handleEffect()    {JsonDocument d;parseBody(d);int f=d["effect"]|0;if(f!=led.effect){effectReset();led.effect=f;}server.send(200,"application/json","{\"ok\":true}");}

// Stopwatch API
void handleSwStart() {
  if (!sw.running) { sw.running=true; sw.startMs=millis(); }
  server.send(200,"application/json","{\"ok\":true}");
}
void handleSwPause() {
  JsonDocument d; parseBody(d);
  if (sw.running) { sw.elapsed=swElapsed(); sw.running=false; }
  // Also accept elapsed override from browser for sync
  if (d.containsKey("elapsed")) sw.elapsed=(unsigned long)(d["elapsed"]|0);
  server.send(200,"application/json","{\"ok\":true}");
}
void handleSwLap()  { triggerAlert(CRGB::Green,4); server.send(200,"application/json","{\"ok\":true}"); }
void handleSwReset(){ sw.running=false; sw.elapsed=0; sw.startMs=0; server.send(200,"application/json","{\"ok\":true}"); }

// Timer API
void handleCtSet(){
  JsonDocument d; parseBody(d);
  unsigned long sec=d["duration"]|60;
  ct.durationMs=sec*1000UL; ct.running=false; ct.finished=false; ct.pausedLeft=0;
  server.send(200,"application/json","{\"ok\":true}");
}
void handleCtStart(){
  if (!ct.running && !ct.finished) {
    unsigned long use=ct.pausedLeft>0?ct.pausedLeft:ct.durationMs;
    ct.durationMs=use; ct.pausedLeft=0; ct.running=true; ct.startMs=millis();
  }
  server.send(200,"application/json","{\"ok\":true}");
}
void handleCtPause(){
  if (ct.running){ ct.pausedLeft=ctRemaining(); ct.running=false; }
  server.send(200,"application/json","{\"ok\":true}");
}
void handleCtReset(){
  ct.running=false; ct.finished=false; ct.pausedLeft=0;
  server.send(200,"application/json","{\"ok\":true}");
}

void handleMatrixMode(){JsonDocument d;parseBody(d);matMode=(MatrixMode)constrain((int)(d["mode"]|0),0,MAT_MODE_COUNT-1);newContent=true;autoCycle=false;lastCycle=millis();server.send(200,"application/json","{\"ok\":true}");}
void handleMatrixText(){JsonDocument d;parseBody(d);strlcpy(customText,d["text"]|"",sizeof(customText));matMode=MAT_CUSTOM;newContent=true;autoCycle=false;server.send(200,"application/json","{\"ok\":true}");}
void handleMatrixAC()  {JsonDocument d;parseBody(d);autoCycle=d["auto"]|true;lastCycle=millis();server.send(200,"application/json","{\"ok\":true}");}
void handleMatrixSpd() {JsonDocument d;parseBody(d);matrix.setSpeed(constrain((int)(d["speed"]|40),10,200));server.send(200,"application/json","{\"ok\":true}");}
void handleMatrixBri() {JsonDocument d;parseBody(d);matrix.setIntensity(constrain((int)(d["brightness"]|5),0,15));server.send(200,"application/json","{\"ok\":true}");}

// ─── MQTT ─────────────────────────────────────────────────────────────────────
#if MQTT_ENABLED
void mqttCallback(char* topic,byte* payload,unsigned int len){
  String msg;for(unsigned int i=0;i<len;i++)msg+=(char)payload[i];
  String t(topic);
  if(t=="led/effect"){led.effect=msg.toInt();effectReset();}
  else if(t=="led/power"){led.on=(msg=="on"||msg=="1");}
  else if(t=="matrix/text"){msg.toCharArray(customText,sizeof(customText));matMode=MAT_CUSTOM;newContent=true;autoCycle=false;}
  else if(t=="matrix/mode"){matMode=(MatrixMode)constrain(msg.toInt(),0,MAT_MODE_COUNT-1);newContent=true;}
}
void mqttConnect(){
  if(!mqtt.connected()&&WiFi.status()==WL_CONNECTED){
    bool ok=strlen(MQTT_USER)?mqtt.connect(MQTT_CLIENT_ID,MQTT_USER,MQTT_PASSWORD):mqtt.connect(MQTT_CLIENT_ID);
    if(ok){mqtt.subscribe("led/effect");mqtt.subscribe("led/power");mqtt.subscribe("matrix/text");mqtt.subscribe("matrix/mode");}
  }
}
#endif

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup(){
  Serial.begin(115200);delay(200);
  Serial.println("\n[BOOT] Starting");

  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,LED_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,MAX_MILLIAMPS);
  FastLED.setBrightness(led.brightness);
  fill_solid(leds,LED_COUNT,CRGB::Black);FastLED.show();

  matrix.begin();
  matrix.setIntensity(MATRIX_BRIGHT);
  matrix.setSpeed(MATRIX_SPEED);
  matrixScroll("Booting...");

  dht.begin();

  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s",WIFI_SSID);
  unsigned long t0=millis();
  while(WiFi.status()!=WL_CONNECTED&&millis()-t0<15000){delay(400);Serial.print(".");}

  if(WiFi.status()==WL_CONNECTED){
    String ip = WiFi.localIP().toString();
    Serial.printf("\n[WiFi] IP: %s\n", ip.c_str());

    // ── Boot display: clearly show IP so user knows where to connect ──────────
    // Step 1: scroll "Connect to: 192.168.x.x" slowly once
    char bootMsg[64];
    snprintf(bootMsg, sizeof(bootMsg), "  Connect to:  %s  ", ip.c_str());
    matrix.setSpeed(70);
    matrixScroll(bootMsg);
    unsigned long scrollStart = millis();
    while (!matrix.displayAnimate() && millis()-scrollStart < 8000) { delay(10); }

    // Step 2: show IP statically centred for 5 seconds — easy to read
    matrix.setSpeed(MATRIX_SPEED);
    matrix.displayClear();
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(ip.c_str());
    Serial.printf("[Boot] IP on matrix: %s\n", ip.c_str());
    delay(5000);
    matrix.setTextAlignment(PA_LEFT);

    // ── NTP ───────────────────────────────────────────────────────────────────
    configTime(TZ_OFFSET_SEC,DST_OFFSET_SEC,NTP_SERVER);
    Serial.print("[NTP] Syncing");
    struct tm ti;int att=0;
    while(!getLocalTime(&ti)&&att<20){delay(500);Serial.print(".");att++;}
    if(getLocalTime(&ti)){
      timeReady=true;
      char tbuf[20];strftime(tbuf,sizeof(tbuf),"%H:%M:%S",&ti);
      Serial.printf("\n[NTP] %s IST\n",tbuf);
    } else Serial.println("\n[NTP] Failed");

  } else {
    Serial.println("\n[WiFi] Failed");
    matrixScroll("No WiFi  check config.h");
  }

  server.on("/",                               handleRoot);
  server.on("/api/status",                     handleApiStatus);
  server.on("/api/power",          HTTP_POST,  handlePower);
  server.on("/api/brightness",     HTTP_POST,  handleBrightness);
  server.on("/api/color",          HTTP_POST,  handleColor);
  server.on("/api/effect",         HTTP_POST,  handleEffect);
  server.on("/api/sw/start",       HTTP_POST,  handleSwStart);
  server.on("/api/sw/pause",       HTTP_POST,  handleSwPause);
  server.on("/api/sw/lap",         HTTP_POST,  handleSwLap);
  server.on("/api/sw/reset",       HTTP_POST,  handleSwReset);
  server.on("/api/ct/set",         HTTP_POST,  handleCtSet);
  server.on("/api/ct/start",       HTTP_POST,  handleCtStart);
  server.on("/api/ct/pause",       HTTP_POST,  handleCtPause);
  server.on("/api/ct/reset",       HTTP_POST,  handleCtReset);
  server.on("/api/matrix/mode",    HTTP_POST,  handleMatrixMode);
  server.on("/api/matrix/text",    HTTP_POST,  handleMatrixText);
  server.on("/api/matrix/autocycle",HTTP_POST, handleMatrixAC);
  server.on("/api/matrix/speed",   HTTP_POST,  handleMatrixSpd);
  server.on("/api/matrix/brightness",HTTP_POST,handleMatrixBri);
  server.begin();
  Serial.println("[HTTP] Server started");

#if MQTT_ENABLED
  mqtt.setServer(MQTT_SERVER,MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqttConnect();
#endif

  delay(2000);readDHT();lastDHTRead=millis();lastCycle=millis();
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop(){
  unsigned long now=millis();
  server.handleClient();

#if MQTT_ENABLED
  if(!mqtt.connected())mqttConnect();
  mqtt.loop();
#endif

  if(now-lastDHTRead>=DHT_INTERVAL){
    readDHT();lastDHTRead=now;
#if MQTT_ENABLED
    if(mqtt.connected()&&!isnan(temperature)){
      char buf[12];
      dtostrf(temperature,5,1,buf);mqtt.publish("sensor/temperature",buf);
      dtostrf(humidity,5,1,buf);mqtt.publish("sensor/humidity",buf);
    }
#endif
  }

  // Countdown timer expiry check
  if(ct.running&&ctRemaining()==0){
    ct.running=false;ct.finished=true;
    triggerAlert(CRGB::Red,10);
    if(matMode==MAT_TIMER)matrixScroll("DONE!");
  }

  handleAlert();
  if(!alertActive){updateMatrix();runEffect();}
}
