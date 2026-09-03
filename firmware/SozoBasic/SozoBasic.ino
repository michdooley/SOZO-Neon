/*
 * SozoBasic — low-maintenance "it just works" looping show.
 * =============================================================================
 * No MIDI, no remote, no serial input. Powers on and runs a fixed, looping
 * stage machine forever (until the auto-shutoff). Every stage is rendered under
 * the global tube-protection dim mask — NOTHING is ever drawn without it.
 *
 * Stages (loop forever):
 *   0  LEVEL TEST   solid at 0/20/40/60/80/100%, 1 s each, twice (12 s)
 *   1  VINTAGE      the composite show (3x flash, 1x dot-seq, 3x flash, 2x dot-par)
 *   2  RANDOM MORPH random flashes morphing rigid -> fading
 *   3  SOLID FADE   fade up to full solid over 6 s, then hold
 *   4  FADE OUT     fade the whole board to black over 2 s
 *   5  GALLERY      cycle the refined pattern bank, 2 min each (one full pass)
 *
 * Auto-shutoff: after SHUTOFF_MS (4 h) all neon turns off. Then it flashes the
 * whole board 10 times in quick succession every 1 min to cut the outlets.
 * The timer is driven by millis() (a hardware timer), so pattern processing
 * never slows it or throws it off. On the Uno R4's onboard 12x8 LED matrix it
 * shows the whole minutes remaining, counting down.
 *
 * Hardware: identical to SozoUnveling/SozoCumulative — Uno R4 WiFi -> 2x PCA9685
 * (0x40 / 0x41) -> 30 neon tubes, active-low PWM.
 * Libraries: Adafruit PWM Servo Driver (+ BusIO); Arduino_LED_Matrix (built in).
 * =============================================================================
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "Arduino_LED_Matrix.h"
#undef NUM_LEDS   // the matrix library #defines NUM_LEDS=96; we use our own (30)

// ============================ TYPES =========================================
// Declared up here so the IDE's auto-generated prototypes can see them.
enum Stage { ST_LEVELTEST, ST_VINTAGE, ST_RANDOM, ST_SOLIDFADE, ST_FADEOUT, ST_GALLERY };
const int NUM_STAGES = 6;
enum GalPhase { GP_IN, GP_SHOW, GP_OUT };

// ============================ CONFIG (edit me) ===============================

// --- PCA9685 PWM boards ---
const uint8_t  NUM_BOARDS  = 2;
const uint16_t PWM_FREQ_HZ = 100;
const bool     INVERT_PWM  = true;          // active-low transformer
const int      NUM_LEDS    = 30;

// LED index (0..29) -> global PCA channel (0..31). From mapping.md.
const uint8_t LED_CHANNEL[NUM_LEDS] = {
  // LED:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
         31, 30, 24, 25, 28, 27,  9, 12, 23, 19, 13,  1,  6,  4,  5,
  // LED: 15  16  17  18  19  20  21  22  23  24  25  26  27  28  29
         26, 29, 22, 18, 11, 10, 16, 15, 14, 17,  3,  2,  8,  0,  7,
};
const int DISABLED_CHANNEL = -1;            // -1 = none disabled

// --- World coordinates (inches), for the spatial patterns ---
const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};
const float YS[NUM_LEDS] = {
   -9.52f, -2.56f,  3.06f,  5.97f,  4.54f,  1.21f,  5.94f, 10.24f, 12.83f, 12.26f,
    9.21f, 13.34f, 16.27f, 16.84f, 14.94f,
  -14.01f, -7.86f, -2.64f,  0.63f,  0.93f, -1.96f,  3.13f,  7.04f,  9.20f,  8.89f,
    6.81f, 10.66f, 13.49f, 14.37f, 13.00f
};

// --- Spatial dim mask (MASTER condition — applied to EVERY stage, always) ---
const float DIM_FLOOR     = 0.10f;
const float TOP_60_POINT  = 8.0f;
const float BOT_60_POINT  = 7.0f;
const float TOP_RAMP_DIST = 6.0f;
const float BOT_RAMP_DIST = 6.0f;

// --- Frame cadence ---
const unsigned long FRAME_MS = 20;          // ~50 fps

// --- Auto-shutoff ---
const unsigned long SHUTOFF_MS = 4UL * 60UL * 60UL * 1000UL;   // 4 hours
// After shutoff: a burst of quick flashes so someone cuts the outlets.
const unsigned long REMINDER_INTERVAL_MS = 1UL * 60UL * 1000UL; // burst every 1 min
const int           REMINDER_FLASHES     = 5;                 // flashes per burst
const unsigned long REMINDER_ON_MS       = 300;                // each flash on
const unsigned long REMINDER_OFF_MS      = 100;                // gap between flashes

// --- Stage durations (ms) ---
const unsigned long LEVELTEST_STEP_MS = 1000;   // 1 s per brightness level
const int           LEVELTEST_LOOPS   = 2;      // run the 6-level ramp twice
const unsigned long VINTAGE_MS        = 120000; // composite show
const unsigned long RANDOM_MS         = 120000; // random morph (also the morph time)
const unsigned long SOLID_FADEIN_MS   = 6000;   // fade up to solid
const unsigned long SOLID_HOLD_MS     = 6000;   // hold full solid
const unsigned long FADEOUT_MS        = 2000;   // fade whole board out
const unsigned long FADEOUT_BLACK_MS  = 2000;   // hold black before gallery
const unsigned long GALLERY_HOLD_MS   = 120000; // 2 min per gallery pattern
const unsigned long GALLERY_FADE_MS   = 5000;   // 5 s dip-to-black between

// --- Composite (vintage) timing ---
const unsigned long FLASH_ON_MS  = 2000;
const unsigned long FLASH_OFF_MS = 600;
const unsigned long DOT_STEP_MS  = 800;

// --- Random-morph look ---
const int   MAX_FLASHES  = 16;
const float FLASH_RATE   = 7.0f;
const float RIGID_ON_S   = 0.18f;
const float FADE_DECAY   = 3.0f;
const float FLASH_LIFE_S = 2.5f;

// ============================ STATE =========================================
Adafruit_PWMServoDriver boards[NUM_BOARDS] = {
  Adafruit_PWMServoDriver(0x40),
  Adafruit_PWMServoDriver(0x41),
};
ArduinoLEDMatrix matrix;

float layer[NUM_LEDS];
float layerPat[NUM_LEDS];

float         fadeLevel  = 1.0f;
float         fadeTarget = 1.0f;
unsigned long fadeMs     = GALLERY_FADE_MS;

unsigned long bootMs     = 0;     // for the shutoff timer
bool          shutoff    = false;
unsigned long shutoffMs  = 0;     // when shutoff fired (for the reminder flash)
int           lastShownMin = -1;

// composite (vintage) ordering: bottom line (15..29) then top line (0..14)
const uint8_t DOT_SEQ_ORDER[NUM_LEDS] = {
  15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
};

// ============================ HELPERS =======================================
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float randf() { return random(10000) / 10000.0f; }
static float wrapMod(float x, float m) { float r = fmod(x, m); return r < 0.0f ? r + m : r; }

void setLed(int i, uint16_t duty12) {
  uint8_t ch = LED_CHANNEL[i];
  if ((int)ch == DISABLED_CHANNEL) duty12 = 0;
  boards[ch / 16].setPin(ch % 16, duty12, INVERT_PWM);
}

float dimGain(int i) {
  float pos, point, rampDist;
  if (i < 15) { pos = i;      point = TOP_60_POINT; rampDist = TOP_RAMP_DIST; }
  else        { pos = i - 15; point = BOT_60_POINT; rampDist = BOT_RAMP_DIST; }
  if (rampDist <= 0) return (pos < point) ? 1.0f : DIM_FLOOR;
  float frac = clamp01((pos - (point - rampDist)) / rampDist);
  return 1.0f - frac * (1.0f - DIM_FLOOR);
}

void writeAll() {
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = clamp01(layer[i]) * dimGain(i);   // dim mask = master, always
    setLed(i, (uint16_t)(clamp01(v) * 4095.0f + 0.5f));
  }
}
void allOff() { for (int i = 0; i < NUM_LEDS; i++) setLed(i, 0); }

void fadeTo(float target, unsigned long ms) { fadeTarget = target; fadeMs = ms ? ms : 1; }
void updateFade(float dtMs) {
  float step = dtMs / (float)fadeMs;
  if      (fadeLevel < fadeTarget) fadeLevel = min(fadeTarget, fadeLevel + step);
  else if (fadeLevel > fadeTarget) fadeLevel = max(fadeTarget, fadeLevel - step);
}

// ============================ COMPOSITE (vintage) ===========================
void buildFlash(unsigned long el, float *out) {
  bool on = (el % (FLASH_ON_MS + FLASH_OFF_MS)) < FLASH_ON_MS;
  for (int i = 0; i < NUM_LEDS; i++) out[i] = on ? 1.0f : 0.0f;
}
void buildDotSeq(unsigned long el, float *out) {
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0.0f;
  out[DOT_SEQ_ORDER[(el / DOT_STEP_MS) % NUM_LEDS]] = 1.0f;
}
void buildDotPar(unsigned long el, float *out) {
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0.0f;
  int step = (el / DOT_STEP_MS) % 15;
  out[15 + step] = 1.0f;   // bottom line
  out[0  + step] = 1.0f;   // top line
}
// 3x FLASH, 1x DOT_SEQ, 3x FLASH, 2x DOT_PARALLEL, looping.
void buildComposite(unsigned long el, float *out) {
  unsigned long flashLoop = FLASH_ON_MS + FLASH_OFF_MS;
  unsigned long seqLoop   = (unsigned long)NUM_LEDS * DOT_STEP_MS;
  unsigned long parLoop   = 15UL * DOT_STEP_MS;
  unsigned long segs[4]   = { 3 * flashLoop, 1 * seqLoop, 3 * flashLoop, 2 * parLoop };
  unsigned long total = segs[0] + segs[1] + segs[2] + segs[3];
  unsigned long c = el % total, acc = 0;
  for (int k = 0; k < 4; k++) {
    if (c < acc + segs[k]) {
      unsigned long le = c - acc;
      if      (k == 0 || k == 2) buildFlash(le, out);
      else if (k == 1)           buildDotSeq(le, out);
      else                       buildDotPar(le, out);
      return;
    }
    acc += segs[k];
  }
}

// ============================ RANDOM MORPH ==================================
struct MFlash { uint8_t led; unsigned long startMs; bool active; };
MFlash mflashes[MAX_FLASHES];
void resetMorph() { for (int i = 0; i < MAX_FLASHES; i++) mflashes[i].active = false; }
void buildRandomMorph(unsigned long now, float morph, float *out) {
  if (randf() < FLASH_RATE * (FRAME_MS / 1000.0f)) {
    for (int s = 0; s < MAX_FLASHES; s++)
      if (!mflashes[s].active) { mflashes[s].active = true; mflashes[s].startMs = now; mflashes[s].led = random(NUM_LEDS); break; }
  }
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0.0f;
  for (int s = 0; s < MAX_FLASHES; s++) {
    if (!mflashes[s].active) continue;
    float age = (now - mflashes[s].startMs) / 1000.0f;
    float rigid = (age < RIGID_ON_S) ? 1.0f : 0.0f;
    float fade  = expf(-age * FADE_DECAY);
    float e = (1.0f - morph) * rigid + morph * fade;
    if (age > FLASH_LIFE_S || (e < 0.004f && age > RIGID_ON_S)) { mflashes[s].active = false; continue; }
    if (e > out[mflashes[s].led]) out[mflashes[s].led] = e;
  }
}

// ===================== GALLERY PATTERN BANK =================================
// Custom (CustomizedPatterns) > refined (Visualizer/refined-patterns) > default.
// stutter_chase removed. Each fills out[0..29] in [0,1] from local time t (s).

void patternBlobGaussian(float t, float out[NUM_LEDS]) {           // default
  const float CX = 2.0f, CY = 5.0f, SIGMA_MIN = 3.0f, SIGMA_MAX = 14.0f, BREATH_HZ = 0.22f;
  float u = sin(t * 2.0f * PI * BREATH_HZ) * 0.5f + 0.5f;
  float sigma = SIGMA_MIN + u * (SIGMA_MAX - SIGMA_MIN);
  float twoSigSq = 2.0f * sigma * sigma;
  for (int i = 0; i < NUM_LEDS; i++) { float dx = XS[i] - CX, dy = YS[i] - CY; out[i] = exp(-(dx*dx + dy*dy) / twoSigSq); }
}
void patternPendulum(float t, float out[NUM_LEDS]) {               // default
  const float SWING_HZ = 0.1f, SWING_AMP = 22.0f, SIGMA = 5.5f, CY = 5.0f;
  float cx = SWING_AMP * sin(t * 2.0f * PI * SWING_HZ);
  float twoSigSq = 2.0f * SIGMA * SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) { float dx = XS[i] - cx, dy = YS[i] - CY; out[i] = exp(-(dx*dx + dy*dy) / twoSigSq); }
}
void patternTide(float t, float out[NUM_LEDS]) {                   // refined
  const float TIDE_HZ = 0.225f, X_HALF = 25.0f, BASE = 0.08f, SWING = 0.92f;
  float tilt = sin(t * 2.0f * PI * TIDE_HZ);
  for (int i = 0; i < NUM_LEDS; i++) {
    float xn = XS[i] / X_HALF; if (xn > 1.0f) xn = 1.0f; else if (xn < -1.0f) xn = -1.0f;
    out[i] = clamp01(BASE + 0.5f * SWING * (1.0f + tilt * xn));
  }
}
void patternSine(float t, float out[NUM_LEDS]) {                   // default
  const float K = 0.35f, OMEGA = 3.0f;
  for (int i = 0; i < NUM_LEDS; i++) out[i] = sin(K * XS[i] + OMEGA * t) * 0.5f + 0.5f;
}
void patternRampAcross(float t, float out[NUM_LEDS]) {             // default
  const float X_MIN = -26.0f, X_MAX = 26.0f, RAMP_WIDTH = 22.0f, SWEEP_PERIOD = 5.0f;
  float u = wrapMod(t / SWEEP_PERIOD, 1.0f);
  float leadX = X_MIN - RAMP_WIDTH + u * (X_MAX - X_MIN + 2.0f * RAMP_WIDTH);
  for (int i = 0; i < NUM_LEDS; i++) { float d = leadX - XS[i]; out[i] = (d >= 0.0f && d <= RAMP_WIDTH) ? (1.0f - d / RAMP_WIDTH) : 0.0f; }
}
void patternChaseV2(float t, float out[NUM_LEDS]) {                // custom (chase_test_v2)
  const int LINE1_LEN = 15;
  const float CHASE_SPEED = 8.0f, CHASE_TAIL = 5.0f, ROW_GAP = 0.2f, REPEAT_GAP = 0.5f;
  float sweepDur = ((LINE1_LEN - 1) + 2.0f * CHASE_TAIL) / CHASE_SPEED;
  float cycle = 2.0f * sweepDur + ROW_GAP + REPEAT_GAP;
  float tc = wrapMod(t, cycle);
  float row2Start = sweepDur + ROW_GAP;
  for (int i = 0; i < NUM_LEDS; i++) {
    int   local = (i < LINE1_LEN) ? i : i - LINE1_LEN;
    float start = (i < LINE1_LEN) ? 0.0f : row2Start;
    float pt = tc - start, v = 0.0f;
    if (pt >= 0.0f && pt < sweepDur) {
      float pos = -CHASE_TAIL + pt * CHASE_SPEED;
      float falloff = 1.0f - fabs((float)local - pos) / CHASE_TAIL;
      v = (falloff > 0.0f) ? falloff : 0.0f;
    }
    out[i] = v;
  }
}
void patternTwoComets(float t, float out[NUM_LEDS]) {              // custom
  const int LINE_LEN = 15;
  const float SPEED_A = 4.05f, SPEED_B = 3.15f, TAIL = 1.56f;
  float posA = wrapMod(t * SPEED_A, (float)LINE_LEN);
  float posB = wrapMod(t * SPEED_B, (float)LINE_LEN);
  for (int i = 0; i < NUM_LEDS; i++) {
    int j = (i < LINE_LEN) ? i : (i - LINE_LEN);
    float pos = (i < LINE_LEN) ? posA : posB;
    float d = fabs((float)j - pos); if (d > LINE_LEN / 2.0f) d = LINE_LEN - d;
    float f = 1.0f - d / TAIL; out[i] = f > 0.0f ? f : 0.0f;
  }
}
void patternBumpCascade(float t, float out[NUM_LEDS]) {            // custom
  const int N_BUMPS = 6;
  const int STARTS[6] = { 0,6,11,15,20,25 }, ENDS[6] = { 5,10,14,19,24,29 }, PEAKS[6] = { 3,8,13,19,23,27 };
  const float PULSE_DUR = 1.35f, STAGGER = 0.228f, REST = 0.7f;
  float cyclePeriod = (N_BUMPS - 1) * STAGGER + PULSE_DUR + REST;
  float cycleT = wrapMod(t, cyclePeriod);
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0.0f;
  for (int p = 0; p < N_BUMPS; p++) {
    float localT = cycleT - p * STAGGER;
    if (localT < 0.0f || localT > PULSE_DUR) continue;
    float amp = sin(localT / PULSE_DUR * PI);
    int rUp = PEAKS[p] - STARTS[p], rDn = ENDS[p] - PEAKS[p];
    float span = (float)((rUp > rDn) ? rUp : rDn) + 1.0f;
    for (int i = STARTS[p]; i <= ENDS[p]; i++) {
      float f = 1.0f - fabs((float)(i - PEAKS[p])) / span; if (f < 0.0f) f = 0.0f;
      float v = amp * f; if (v > out[i]) out[i] = v;
    }
  }
}
void patternRipple(float t, float out[NUM_LEDS]) {                 // default
  const float OX = 18.2f, OY = 10.66f, RING_W = 5.88f, RING_HZ = 0.5527f, RING_VEL = 19.74f, MAX_R = 72.0f;
  const int N_RINGS = 9;
  float ringPeriod = 1.0f / RING_HZ, base = wrapMod(t, ringPeriod);
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY, d = sqrt(dx*dx + dy*dy), bright = 0.0f;
    for (int k = 0; k < N_RINGS; k++) {
      float r = (base + k * ringPeriod) * RING_VEL; if (r > MAX_R) continue;
      float diff = fabs(d - r); if (diff > RING_W) continue;
      float v = (1.0f - diff / RING_W) * (1.0f - r / MAX_R); if (v > bright) bright = v;
    }
    out[i] = bright;
  }
}
void patternRecedeFill(float t, float out[NUM_LEDS]) {             // default
  const float OX = 0.0f, OY = 0.0f, PERIOD = 10.8f, MAX_R = 50.0f, EDGE = 3.0f;
  float u = wrapMod(t, PERIOD) / PERIOD;
  float tri = (u < 0.5f) ? (u * 2.0f) : (2.0f - u * 2.0f);
  float r = tri * tri * (3.0f - 2.0f * tri) * MAX_R;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY, d = sqrt(dx*dx + dy*dy);
    if (d <= r) out[i] = 1.0f; else if (d <= r + EDGE) out[i] = 1.0f - (d - r) / EDGE; else out[i] = 0.0f;
  }
}
void patternConvergence(float t, float out[NUM_LEDS]) {            // default
  const float OX = 18.2f, OY = 10.66f, MAX_R = 46.0f, CYCLE = 4.0f, FLASH = 0.5f, RING_W = 4.0f;
  float u = wrapMod(t, CYCLE + FLASH);
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY, d = sqrt(dx*dx + dy*dy), v = 0.0f;
    if (u < CYCLE) { float r = MAX_R * (1.0f - u / CYCLE), diff = fabs(d - r); if (diff < RING_W) v = 1.0f - diff / RING_W; }
    else { float center = 1.0f - (u - CYCLE) / FLASH; v = center * exp(-d * d / (2.0f * 4.0f * 4.0f)); }
    out[i] = clamp01(v);
  }
}
void patternBouncingUp(float t, float out[NUM_LEDS]) {             // custom (new)
  const int LINE1_LEN = 15, N_BALLS = 2;
  const float SPEED_A = 9.0f, SPEED_B = 7.0f, SPEED_JIT = 0.12f, BALL_SIZE = 1.0f;
  float span = (float)LINE1_LEN;
  for (int i = 0; i < NUM_LEDS; i++) {
    int   j    = (i < LINE1_LEN) ? i : i - LINE1_LEN;
    float s    = (i < LINE1_LEN) ? SPEED_A : SPEED_B;
    float seed = (i < LINE1_LEN) ? 0.0f : 11.0f;
    float v = 0.0f;
    for (int k = 0; k < N_BALLS; k++) {
      float h = sin(((float)k + 1.0f) * 12.9898f + seed) * 43758.5453f; h = h - floor(h);
      float sk = s * (1.0f + (h - 0.5f) * 2.0f * SPEED_JIT);
      float offset = (float)k / (float)N_BALLS * span;
      float pos = wrapMod(t * sk + offset, span);
      float f = 1.0f - fabs((float)j - pos) / BALL_SIZE;
      if (f > v) v = f;
    }
    out[i] = v;
  }
}
// random_flash: custom (new), stateful.
struct RFlash { uint8_t led; float startT; bool active; };
RFlash rflashes[8];
void initRandomFlash() { for (int i = 0; i < 8; i++) rflashes[i].active = false; }
void patternRandomFlash(float t, float out[NUM_LEDS]) {            // custom (new)
  const int MAXF = 8;
  const float DECAY = 4.5f, RATE = 6.0f;
  if (randf() < RATE * (FRAME_MS / 1000.0f)) {
    for (int s = 0; s < MAXF; s++) if (!rflashes[s].active) { rflashes[s].led = random(NUM_LEDS); rflashes[s].startT = t; rflashes[s].active = true; break; }
  }
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0.0f;
  for (int s = 0; s < MAXF; s++) {
    if (!rflashes[s].active) continue;
    float age = t - rflashes[s].startT;
    if (age * DECAY > 5.5f) { rflashes[s].active = false; continue; }
    float v = expf(-age * DECAY);
    if (v > out[rflashes[s].led]) out[rflashes[s].led] = v;
  }
}

struct GalPattern { const char *name; void (*fn)(float, float *); void (*init)(); };
const GalPattern GAL_PATTERNS[] = {
  { "blob_gaussian", patternBlobGaussian, NULL },
  { "pendulum",      patternPendulum,     NULL },
  { "tide",          patternTide,         NULL },
  { "sine_wave",     patternSine,         NULL },
  { "ramp_across",   patternRampAcross,   NULL },
  { "chase_v2",      patternChaseV2,      NULL },
  { "two_comets",    patternTwoComets,    NULL },
  { "bump_cascade",  patternBumpCascade,  NULL },
  { "ripple",        patternRipple,       NULL },
  { "recede_fill",   patternRecedeFill,   NULL },
  { "convergence",   patternConvergence,  NULL },
  { "bouncing_up",   patternBouncingUp,   NULL },
  { "random_flash",  patternRandomFlash,  initRandomFlash },
};
const int GAL_PATTERN_COUNT = sizeof(GAL_PATTERNS) / sizeof(GAL_PATTERNS[0]);

// ============================ STAGE MACHINE =================================
Stage         stage      = ST_LEVELTEST;
unsigned long stageStart = 0;

// gallery sub-state
GalPhase      galPhase   = GP_IN;
int           galIdx     = 0;
int           galShown   = 0;     // patterns shown this pass
unsigned long galPatStart = 0;

const float LEVELS[6] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };

void galleryStartPattern(unsigned long now) {
  galPatStart = now;
  if (GAL_PATTERNS[galIdx].init) GAL_PATTERNS[galIdx].init();
}

void enterStage(Stage s, unsigned long now) {
  stage = s; stageStart = now;
  switch (s) {
    case ST_LEVELTEST: fadeLevel = fadeTarget = 1.0f; break;
    case ST_VINTAGE:   fadeLevel = fadeTarget = 1.0f; break;
    case ST_RANDOM:    fadeLevel = fadeTarget = 1.0f; resetMorph(); break;
    case ST_SOLIDFADE: fadeLevel = 0.0f; fadeTo(1.0f, SOLID_FADEIN_MS); break;
    case ST_FADEOUT:   fadeLevel = 1.0f; fadeTo(0.0f, FADEOUT_MS); break;
    case ST_GALLERY:
      galPhase = GP_IN; galIdx = 0; galShown = 0;
      galleryStartPattern(now);
      fadeLevel = 0.0f; fadeTo(1.0f, GALLERY_FADE_MS);
      break;
  }
  Serial.print(F("# STAGE -> ")); Serial.println((int)s);
}
void advanceStage(unsigned long now) { enterStage((Stage)(((int)stage + 1) % NUM_STAGES), now); }

void tickGallery(unsigned long now) {
  switch (galPhase) {
    case GP_IN:   if (fadeLevel >= 0.999f) { galPatStart = now; galPhase = GP_SHOW; } break;
    case GP_SHOW: if (now - galPatStart >= GALLERY_HOLD_MS) { fadeTo(0.0f, GALLERY_FADE_MS); galPhase = GP_OUT; } break;
    case GP_OUT:
      if (fadeLevel <= 0.001f) {
        galShown++;
        if (galShown >= GAL_PATTERN_COUNT) { advanceStage(now); return; }   // one full pass done
        galIdx = (galIdx + 1) % GAL_PATTERN_COUNT;
        galleryStartPattern(now);
        fadeTo(1.0f, GALLERY_FADE_MS); galPhase = GP_IN;
      }
      break;
  }
}

void tickStage(unsigned long now) {
  unsigned long el = now - stageStart;
  switch (stage) {
    case ST_LEVELTEST: if (el >= LEVELTEST_STEP_MS * 6 * LEVELTEST_LOOPS) advanceStage(now); break;
    case ST_VINTAGE:   if (el >= VINTAGE_MS) advanceStage(now); break;
    case ST_RANDOM:    if (el >= RANDOM_MS) advanceStage(now); break;
    case ST_SOLIDFADE: if (el >= SOLID_FADEIN_MS + SOLID_HOLD_MS) advanceStage(now); break;
    case ST_FADEOUT:   if (el >= FADEOUT_MS + FADEOUT_BLACK_MS) advanceStage(now); break;
    case ST_GALLERY:   tickGallery(now); break;
  }
}

void renderStage(unsigned long now) {
  unsigned long el = now - stageStart;
  switch (stage) {
    case ST_LEVELTEST: {
      int step = (int)(el / LEVELTEST_STEP_MS);
      float lv = LEVELS[step % 6];
      for (int i = 0; i < NUM_LEDS; i++) layer[i] = lv;
      break;
    }
    case ST_VINTAGE:
      buildComposite(el, layer);
      for (int i = 0; i < NUM_LEDS; i++) layer[i] *= fadeLevel;
      break;
    case ST_RANDOM:
      buildRandomMorph(now, clamp01((float)el / (float)RANDOM_MS), layer);
      for (int i = 0; i < NUM_LEDS; i++) layer[i] *= fadeLevel;
      break;
    case ST_SOLIDFADE:
    case ST_FADEOUT:
      for (int i = 0; i < NUM_LEDS; i++) layer[i] = fadeLevel;   // solid * fade
      break;
    case ST_GALLERY:
      GAL_PATTERNS[galIdx].fn((now - galPatStart) / 1000.0f, layerPat);
      for (int i = 0; i < NUM_LEDS; i++) layer[i] = clamp01(layerPat[i]) * fadeLevel;
      break;
  }
}

// ============================ ONBOARD MATRIX (countdown) ====================
// 3x5 digit glyphs (low 3 bits per row).
const uint8_t DIGIT[10][5] = {
  {0b111,0b101,0b101,0b101,0b111}, {0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111}, {0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001}, {0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111}, {0b111,0b001,0b010,0b100,0b100},
  {0b111,0b101,0b111,0b101,0b111}, {0b111,0b101,0b111,0b001,0b111},
};
void showNumber(int n) {
  if (n < 0) n = 0;
  int digs[3], nd = 0;
  if (n >= 100) digs[nd++] = (n / 100) % 10;
  if (n >= 10)  digs[nd++] = (n / 10) % 10;
  digs[nd++] = n % 10;
  int width = nd * 3 + (nd - 1);
  int startX = (12 - width) / 2; if (startX < 0) startX = 0;
  uint8_t frame[8][12];
  for (int y = 0; y < 8; y++) for (int x = 0; x < 12; x++) frame[y][x] = 0;
  int x = startX;
  for (int d = 0; d < nd; d++) {
    for (int row = 0; row < 5; row++) {
      uint8_t bits = DIGIT[digs[d]][row];
      for (int col = 0; col < 3; col++)
        if (bits & (1 << (2 - col))) { int px = x + col, py = 1 + row; if (px >= 0 && px < 12 && py >= 0 && py < 8) frame[py][px] = 1; }
    }
    x += 4;
  }
  matrix.renderBitmap(frame, 8, 12);
}

// ============================ SETUP / LOOP ==================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  for (uint8_t b = 0; b < NUM_BOARDS; b++) {
    boards[b].begin();
    boards[b].setOscillatorFrequency(27000000);
    boards[b].setPWMFreq(PWM_FREQ_HZ);
  }
  Wire.setClock(400000);
  matrix.begin();

  randomSeed(analogRead(A0));
  resetMorph();
  initRandomFlash();
  for (int i = 0; i < NUM_LEDS; i++) layer[i] = 0;
  allOff();

  bootMs = millis();
  enterStage(ST_LEVELTEST, bootMs);
  showNumber((int)(SHUTOFF_MS / 60000UL));
  Serial.println(F("SozoBasic running. Serial debug enabled — send '?' for the command list."));
}

// ============================ SERIAL DEBUG (timer) ==========================
// Lets you troubleshoot the end-of-timer behavior without waiting hours.
char cmdBuf[24]; int cmdLen = 0;

void printStatus() {
  unsigned long now = millis();
  if (shutoff) {
    Serial.print(F("# SHUTOFF, ")); Serial.print((now - shutoffMs) / 1000UL);
    Serial.println(F("s since; reminder flash every minute"));
  } else {
    unsigned long el = now - bootMs;
    long rem = (el < SHUTOFF_MS) ? (long)((SHUTOFF_MS - el) / 60000UL) : 0;
    Serial.print(F("# running, ~")); Serial.print(rem);
    Serial.print(F(" min to shutoff, stage ")); Serial.println((int)stage);
  }
}

// Jump the shutoff clock so `mins` minutes remain (skip 0 = shut off now).
void doSkip(long mins) {
  if (mins < 0) mins = 0;
  unsigned long remain = (unsigned long)mins * 60000UL;
  if (remain > SHUTOFF_MS) remain = SHUTOFF_MS;
  shutoff = false;
  bootMs = millis() - (SHUTOFF_MS - remain);
  lastShownMin = -1;
  Serial.print(F("# skip -> ")); Serial.print(mins); Serial.println(F(" min remaining"));
}

// Force shutoff and make the reminder burst fire within ~1 s (to eyeball it).
void doFlashTest() {
  shutoff = true;
  shutoffMs = millis() - REMINDER_INTERVAL_MS + 800;
  Serial.println(F("# flash test (shutoff; 10-flash burst imminent)"));
}

void doReset() {
  shutoff = false; bootMs = millis(); lastShownMin = -1;
  Serial.println(F("# timer reset (full duration)"));
}

// Advance to the next top-level stage (level test -> vintage -> ... -> gallery).
void nextStage() { shutoff = false; advanceStage(millis()); }

// Jump straight to a gallery pattern, shown instantly at full (skips the fade).
void showGalleryPattern(int idx) {
  unsigned long now = millis();
  shutoff = false;
  if (stage != ST_GALLERY) enterStage(ST_GALLERY, now);   // galIdx -> 0
  galIdx = (idx >= 0) ? (idx % GAL_PATTERN_COUNT) : galIdx;
  galleryStartPattern(now);
  fadeLevel = fadeTarget = 1.0f;
  galPhase = GP_SHOW; galPatStart = now;
  Serial.print(F("# pattern -> ")); Serial.print(galIdx); Serial.print(' ');
  Serial.println(GAL_PATTERNS[galIdx].name);
}
// Next gallery pattern (enters gallery first if needed).
void nextPattern() {
  if (stage == ST_GALLERY) showGalleryPattern((galIdx + 1) % GAL_PATTERN_COUNT);
  else                     showGalleryPattern(0);
}
void listPatterns() {
  for (int i = 0; i < GAL_PATTERN_COUNT; i++) { Serial.print(F("#  ")); Serial.print(i); Serial.print(' '); Serial.println(GAL_PATTERNS[i].name); }
}

void printHelp() {
  Serial.println(F("# commands:"));
  Serial.println(F("#   skip <mins>  jump timer to N min remaining (0 = shut off now)"));
  Serial.println(F("#   off          shut off now      flash  test a reminder flash"));
  Serial.println(F("#   reset        restart 4h timer  status show timer/stage"));
  Serial.println(F("#   stage        next stage        next   next gallery pattern"));
  Serial.println(F("#   pat <n>      gallery pattern n  list   list gallery patterns"));
}

void handleCmd(char *line) {
  while (*line == ' ') line++;
  if      (!strncmp(line, "skip", 4)) doSkip(atol(line + 4));
  else if (!strcmp(line, "off") || !strcmp(line, "shutoff")) doSkip(0);
  else if (!strcmp(line, "flash"))    doFlashTest();
  else if (!strcmp(line, "reset"))    doReset();
  else if (!strcmp(line, "status") || !strcmp(line, "s")) printStatus();
  else if (!strcmp(line, "next") || !strcmp(line, "n"))    nextPattern();
  else if (!strcmp(line, "stage"))    nextStage();
  else if (!strncmp(line, "pat", 3))  showGalleryPattern(atoi(line + 3));
  else if (!strcmp(line, "list") || !strcmp(line, "l"))    listPatterns();
  else printHelp();
}

void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { if (cmdLen > 0) { cmdBuf[cmdLen] = '\0'; handleCmd(cmdBuf); cmdLen = 0; } }
    else if (cmdLen < (int)sizeof(cmdBuf) - 1) cmdBuf[cmdLen++] = c;
    else cmdLen = 0;
  }
}

unsigned long lastFrameMs = 0;

void loop() {
  unsigned long now = millis();
  pollSerial();

  // --- auto-shutoff (millis-driven; independent of render load) ---
  if (!shutoff && (now - bootMs) >= SHUTOFF_MS) {
    shutoff = true;
    shutoffMs = now;
    allOff();
    showNumber(0);
    Serial.println(F("# SHUTOFF — neon off (reminder: 10 flashes every minute)."));
  }
  if (shutoff) {
    // A burst of REMINDER_FLASHES quick all-on flashes every REMINDER_INTERVAL_MS
    // (under the dim mask). Only writes the boards on the on/off edges.
    static bool reminderOn = false;
    unsigned long since = now - shutoffMs;
    unsigned long cyc   = REMINDER_ON_MS + REMINDER_OFF_MS;
    unsigned long phase = since % REMINDER_INTERVAL_MS;
    bool wantOn = (since >= REMINDER_INTERVAL_MS) &&
                  (phase < (unsigned long)REMINDER_FLASHES * cyc) &&
                  ((phase % cyc) < REMINDER_ON_MS);
    if (wantOn != reminderOn) {
      reminderOn = wantOn;
      if (wantOn) { for (int i = 0; i < NUM_LEDS; i++) layer[i] = 1.0f; writeAll(); }
      else allOff();
    }
    return;
  }

  // --- matrix countdown: minutes remaining, refreshed when it changes ---
  int minsLeft = (int)((SHUTOFF_MS - (now - bootMs)) / 60000UL);
  if (minsLeft != lastShownMin) { lastShownMin = minsLeft; showNumber(minsLeft); }

  if (now - lastFrameMs >= FRAME_MS) {
    float dtMs = now - lastFrameMs; if (dtMs > 100.0f) dtMs = 100.0f;
    lastFrameMs = now;

    tickStage(now);
    updateFade(dtMs);
    renderStage(now);
    writeAll();
  }
}
