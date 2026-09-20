#pragma once
#include <FastLED.h>
#include "config.h"

extern CRGB leds[];

static uint8_t  eff_hue  = 0;
static uint16_t eff_step = 0;

inline void effectReset() { eff_hue = 0; eff_step = 0; }

// ─────────────────────────────────────────────────────────────────────────────
void fxSolid(CRGB color) {
  fill_solid(leds, LED_COUNT, color);
}

void fxBreathing(CRGB color) {
  uint8_t b = beatsin8(6, 20, 255);
  fill_solid(leds, LED_COUNT, color.nscale8(b));
}

void fxColorCycle() {
  fill_solid(leds, LED_COUNT, CHSV(eff_hue++, 255, 200));
  delay(15);
}

void fxRainbow() {
  fill_rainbow(leds, LED_COUNT, eff_hue++, 7);
  delay(10);
}

void fxRainbowCycle() {
  fill_rainbow(leds, LED_COUNT, eff_hue, 256 / LED_COUNT);
  eff_hue++;
  delay(8);
}

void fxColorWipe(CRGB color) {
  static int pos = 0;
  static bool wiping = true;
  if (wiping) { leds[pos++] = color; if (pos >= LED_COUNT) { pos = LED_COUNT-1; wiping = false; } }
  else        { leds[pos--] = CRGB::Black; if (pos < 0)  { pos = 0; wiping = true; } }
  delay(30);
}

void fxTheaterChase(CRGB color) {
  fill_solid(leds, LED_COUNT, CRGB::Black);
  for (int i = eff_step % 3; i < LED_COUNT; i += 3) leds[i] = color;
  eff_step++;
  delay(80);
}

void fxRunningLights(CRGB color) {
  for (int i = 0; i < LED_COUNT; i++) {
    float level = (sin(((i + eff_step) * 3.14159f * 2) / LED_COUNT) + 1.0f) / 2.0f;
    leds[i] = color.nscale8((uint8_t)(level * 255));
  }
  eff_step++;
  delay(20);
}

void fxLarson(CRGB color) {
  static int pos = 0, dir = 1;
  fadeToBlackBy(leds, LED_COUNT, 60);
  leds[pos] = color;
  if (pos > 0)            leds[pos-1] = color.nscale8(80);
  if (pos < LED_COUNT-1)  leds[pos+1] = color.nscale8(80);
  pos += dir;
  if (pos >= LED_COUNT-1 || pos <= 0) dir = -dir;
  delay(30);
}

void fxTwinkle(CRGB color) {
  fadeToBlackBy(leds, LED_COUNT, 20);
  leds[random16(LED_COUNT)] = color;
  delay(40);
}

void fxSparkle(CRGB base) {
  fill_solid(leds, LED_COUNT, base);
  leds[random16(LED_COUNT)] = CRGB::White;
  delay(30);
}

static uint8_t heat[LED_COUNT];
void fxFire() {
  for (int i = 0; i < LED_COUNT; i++)
    heat[i] = qsub8(heat[i], random8(0, ((55*10)/LED_COUNT)+2));
  for (int i = LED_COUNT-1; i >= 2; i--)
    heat[i] = (heat[i-1] + heat[i-2] + heat[i-2]) / 3;
  if (random8() < 120) { int y = random8(7); heat[y] = qadd8(heat[y], random8(160,255)); }
  for (int i = 0; i < LED_COUNT; i++) leds[i] = HeatColor(heat[i]);
  delay(15);
}

void fxComet(CRGB color) {
  static int pos = 0;
  fadeToBlackBy(leds, LED_COUNT, 80);
  for (int i = 0; i < 5; i++) {
    int p = pos - i;
    if (p >= 0 && p < LED_COUNT) leds[p] = color.nscale8(255 - i*50);
  }
  if (++pos >= LED_COUNT+5) pos = 0;
  delay(20);
}

void fxStrobe(CRGB color) {
  fill_solid(leds, LED_COUNT, color); FastLED.show(); delay(50);
  fill_solid(leds, LED_COUNT, CRGB::Black); FastLED.show(); delay(200);
}

void fxSunrise() {
  static uint8_t step = 0;
  CRGB c;
  if      (step < 85)  c = CRGB(step*3, 0, 0);
  else if (step < 170) c = CRGB(255, (step-85)*2, 0);
  else                 c = CRGB(255, 170+(step-170), (step-170));
  fill_solid(leds, LED_COUNT, c);
  if (step < 255) step++;
  delay(60);
}

void fxMeteor(CRGB color) {
  static int pos = 0;
  fadeToBlackBy(leds, LED_COUNT, 64);
  for (int i = 0; i < 8; i++) {
    int p = pos - i;
    if (p >= 0 && p < LED_COUNT) leds[p] = color.nscale8(255 - i*(255/8));
  }
  if (++pos >= LED_COUNT+8) pos = 0;
  delay(15);
}

void fxConfetti() {
  fadeToBlackBy(leds, LED_COUNT, 10);
  leds[random16(LED_COUNT)] += CHSV(eff_hue + random8(64), 200, 255);
  eff_hue++;
  delay(10);
}

void fxPolice() {
  static uint8_t tick = 0;
  fill_solid(leds, LED_COUNT, CRGB::Black);
  int half = LED_COUNT / 2;
  if (tick < 10) for (int i=0;i<half;i++)      leds[i] = CRGB::Red;
  else           for (int i=half;i<LED_COUNT;i++) leds[i] = CRGB::Blue;
  tick = (tick+1) % 20;
  delay(50);
}

void fxCandle() {
  for (int i = 0; i < LED_COUNT; i++) {
    uint8_t f = random8(180, 255);
    leds[i] = CRGB(f, f/3, 0);
  }
  delay(random8(20, 80));
}

void fxWipeRandom() {
  static int pos = 0;
  static CRGB col = CRGB::Red;
  leds[pos] = col;
  if (++pos >= LED_COUNT) { pos = 0; col = CHSV(random8(), 255, 200); }
  delay(25);
}
