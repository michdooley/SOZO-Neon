/*
 * SozoUnveling — performance state machine for the sculpture unveiling.
 * =============================================================================
 * Driven by MIDI notes from Ableton over serial:
 *   "start,<tt>,<nn>,<vv>"   note on  (tt = track/channel, nn = note, vv = vel)
 *   "stop,<tt>,<nn>"         note off
 *   "cc,<num>,<val>"         control change (cc 123,0 = all notes off)
 * The track field lets you split instruments; see MIDI_MAPPED below.
 *
 * Hardware is identical to SozoCumulative: Arduino Uno R4 WiFi -> 2x PCA9685
 * (0x40 / 0x41) -> 30 neon tubes, active-low PWM dimming. The localized
 * "tube-protection" dim mask is a MASTER condition applied to EVERY scene.
 *
 * -------------------------- SCENE TRIGGERS (notes) ---------------------------
 * A note-on on one of these trigger notes switches scene, from any state:
 *   note 0   SOLID         all tubes on, static (under the dim mask)
 *   note 1   COMPOSITE     a sequenced show, looping:
 *                            3x FLASH, 1x DOT_SEQ, 3x FLASH, 2x DOT_PARALLEL
 *   note 4   RANDOM_MORPH  random flashes that morph from rigid -> fading (5 min)
 *   note 5   MIDI_MAPPED   performance: pitch mapped diagonally (bottom-left ->
 *                          top-right), velocity-sensitive. Split by track:
 *                            ch 2 -> LEDs 15..25 (bottom line's first 2 humps
 *                                    + bottom dot of the 3rd hump)   [bass]
 *                            ch 5 -> LEDs 0..14 + 26..29 (top line + rest of
 *                                    the bottom line's top hump)     [chords]
 *   note 6   GALLERY       enter gallery mode (see below)
 *
 * Notes >= PERF_NOTE_MIN (12) are PERFORMANCE notes (the actual keys played).
 * They are diagonally mapped in MIDI_MAPPED, randomly assigned in GALLERY, and
 * ignored in the other scenes. (Scene triggers stay low so a keyboard, which
 * sends notes >= ~36, never collides with them.)
 *
 * ------------------------------- GALLERY MODE --------------------------------
 * Loops the playlist, 2 min per pattern, dipping through black (5 s out + 5 s
 * in). If a performance note arrives, the current pattern fades out (5 s) and
 * the notes are performed in RANDOM-assignment MIDI. After 30 s with no notes,
 * it fades (5 s) back into the pattern it left.
 *
 * NOTE: scene assignments, the control/performance split, the line ordering,
 * the diagonal metric, the gallery playlist, and the mapped-note range are all
 * constants below — tweak to taste.
 * =============================================================================
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ============================ CONFIG (edit me) ===============================

// --- PCA9685 PWM boards ---
const uint8_t  NUM_BOARDS  = 2;
const uint16_t PWM_FREQ_HZ = 100;
const bool     INVERT_PWM  = true;          // active-low transformer
const int      NUM_LEDS    = 30;

// LED index (0..29) -> global PCA channel (0..31). From mapping.md (same as
// SozoCumulative). Channels 20 & 21 are unplugged and never referenced.
const uint8_t LED_CHANNEL[NUM_LEDS] = {
  // LED:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
         31, 30, 24, 25, 28, 27,  9, 12, 23, 19, 13,  1,  6,  4,  5,
  // LED: 15  16  17  18  19  20  21  22  23  24  25  26  27  28  29
         26, 29, 22, 18, 11, 10, 16, 15, 14, 17,  3,  2,  8,  0,  7,
};
const int DISABLED_CHANNEL = -1;            // -1 = none disabled

// --- World coordinates (inches), for the diagonal MIDI map ---
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

// --- Spatial dim mask (MASTER condition, applied to every scene) ---
const float DIM_FLOOR     = 0.10f;   // floor for the small tubes (0..1)
const float TOP_60_POINT  = 8.0f;    // line 1: LEDs in to reach DIM_FLOOR
const float BOT_60_POINT  = 7.0f;    // line 2: LEDs in (from idx 15)
const float TOP_RAMP_DIST = 6.0f;    // line 1: ramp length
const float BOT_RAMP_DIST = 6.0f;    // line 2: ramp length

// --- Scene-trigger notes ---
// Live show set: 0 (solid), 1 (composite), 4 (random), 5 (MIDI), 6 (gallery).
// The individual flash/dot patterns are no longer directly triggerable (they
// still appear inside the composite + gallery) to avoid accidental triggers.
const uint8_t NOTE_SOLID      = 0;
const uint8_t NOTE_COMPOSITE  = 1;
const uint8_t NOTE_RANDOM     = 2;    // random-flash morph (rigid -> fade over 5 min)
const uint8_t NOTE_SOLID_FADE = 3;    // 6 s fade-in to a full solid
const uint8_t NOTE_FADE_OUT   = 4;    // 2 s fade-out of the whole board
const uint8_t NOTE_MIDI       = 5;
const uint8_t NOTE_GALLERY    = 6;
// Notes 0..CONTROL_NOTE_MAX are scene/control on EVERY channel, at all times —
// handled before (and never confused with) channel-routed performance notes.
const uint8_t CONTROL_NOTE_MAX = 10;
const uint8_t PERF_NOTE_MIN  = 12;    // notes >= this (and not a trigger) are performance

// --- Timing (ms) ---
const unsigned long FRAME_MS        = 20;       // ~50 fps
const unsigned long FLASH_ON_MS     = 2000;     // vintage flash on
const unsigned long FLASH_OFF_MS    = 600;     // vintage flash off
const unsigned long DOT_STEP_MS     = 800;      // dot advance interval
const unsigned long RANDOM_MORPH_MS = 300000;   // 5 min rigid -> fade
const unsigned long GALLERY_HOLD_MS = 120000;   // 2 min per pattern
const unsigned long GALLERY_FADE_MS = 5000;     // 5 s fades
const unsigned long MIDI_IDLE_MS    = 30000;    // 30 s idle -> back to gallery
const unsigned long SOLID_FADE_IN_MS  = 6000;   // note 3: fade up to solid
const unsigned long BOARD_FADE_OUT_MS = 2000;   // note 4: fade whole board out

// --- Random-flash look ---
const int   MAX_FLASHES = 16;
const float FLASH_RATE  = 7.0f;     // flashes/sec
const float RIGID_ON_S  = 0.18f;    // rigid square on-time
const float FADE_DECAY  = 3.0f;     // fade exp decay (1/s)
const float FLASH_LIFE_S = 2.5f;    // hard retire age

// --- Diagonal MIDI map note range ---
const uint8_t MAP_NOTE_LOW  = 36;   // -> bottom-left LED (of the channel's region)
const uint8_t MAP_NOTE_HIGH = 84;   // -> top-right LED

// --- MIDI_MAPPED channels (the %tt% field in "start,tt,nn,vv") -> LED region ---
// Channel 2 (bass):   bottom line's first two humps + the bottom dot of its last
//                     hump  -> LEDs 15..25.
// Channel 5 (chords): top line (0..14) + the rest of the bottom line's top hump
//                     (26..29).   [ch 5 keeps chords clear of a bass overlap]
// Within each region, pitch maps diagonally (bottom-left -> top-right). Any
// other track number falls back to the full-board diagonal.
// These are the %tt% values your DAW sends for each channel. MIDI channels are
// 1-indexed, so 1 and 2. If your bridge sends the raw 0-indexed nibble instead,
// set these to 0 and 1 (watch the "# MAP ch=" debug line to see what arrives).
const int     CH1_TRACK  = 2;       // bass   -> LEDs 15..25           (channel 2 this setup)
const int     CH2_TRACK  = 5;       // chords -> LEDs 0..14 + 26..29   (channel 5 this setup)
const uint8_t MAX_TRACKS = 8;       // held notes are keyed per (track, note)
const bool    DEBUG_MIDI = true;    // print each mapped note: "# MAP ch=.. note=.. -> LED .."

// Bass channel uses a DIRECT mapping (not the diagonal spread): each semitone
// lights the next bass LED along the line (15..25), wrapping. BASS_NOTE_BASE
// -> LED 15; notes below/above wrap cyclically around the bass humps. The bulk
// playing range 34..46 covers all 11 bass LEDs (34..44 are 1:1; 45,46 wrap).
const uint8_t BASS_NOTE_BASE = 34;  // note 34 -> first bass LED (15)
const uint8_t BASS_LED_LOW   = 15;  // first bass LED (path order up the line)
const uint8_t BASS_LED_COUNT = 11;  // LEDs 15..25

// --- ADSR (could be exposed via CC 20-23 later) ---
unsigned long attackMs  = 8;
unsigned long decayMs   = 120;
float         sustainLvl = 0.85f;
unsigned long releaseMs = 600;

// ============================ TYPES =========================================
enum Scene {
  SC_IDLE, SC_SOLID, SC_COMPOSITE, SC_FLASH, SC_DOT_SEQ, SC_DOT_PAR, SC_RANDOM,
  SC_MIDI_MAPPED, SC_GALLERY
};

// Dot-sequential order: bottom line (Line 2, idx 15..29) then top line (idx 0..14).
const uint8_t DOT_SEQ_ORDER[NUM_LEDS] = {
  15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
};

// --- COMPOSITE (note 1): a fixed sequence of sub-patterns, each repeated N
// loops, then the whole thing loops. From the spec:
//   3x FLASH, 1x DOT_SEQ, 3x FLASH, 2x DOT_PARALLEL
struct Seg { Scene pat; uint8_t loops; };
const Seg COMPOSITE_SEQ[] = {
  { SC_FLASH,   3 },
  { SC_DOT_SEQ, 1 },
  { SC_FLASH,   3 },
  { SC_DOT_PAR, 2 },
};
const int COMPOSITE_LEN = sizeof(COMPOSITE_SEQ) / sizeof(COMPOSITE_SEQ[0]);

// One full loop of each building-block pattern (ms) — used to time COMPOSITE.
unsigned long patternLoopMs(Scene s) {
  switch (s) {
    case SC_FLASH:   return FLASH_ON_MS + FLASH_OFF_MS;
    case SC_DOT_SEQ: return (unsigned long)NUM_LEDS * DOT_STEP_MS;   // 30 steps
    case SC_DOT_PAR: return 15UL * DOT_STEP_MS;                      // 15 steps
    default:         return 1000;
  }
}

// Gallery sub-phase.
enum GalPhase { GP_SHOW, GP_OUT_NEXT, GP_IN, GP_TO_MIDI, GP_MIDI };

// ============================ STATE =========================================
Adafruit_PWMServoDriver boards[NUM_BOARDS] = {
  Adafruit_PWMServoDriver(0x40),
  Adafruit_PWMServoDriver(0x41),
};

Scene         scene       = SC_IDLE;
unsigned long sceneStart  = 0;
unsigned long lastFrameMs = 0;

float layer[NUM_LEDS];        // final per-LED brightness 0..1 (pre dim-mask)
float layerPat[NUM_LEDS];     // scratch: pattern contribution
float layerMidi[NUM_LEDS];    // scratch: MIDI contribution

// Master fade (1 = full). Performance scenes hold it at 1; gallery animates it.
float         fadeLevel  = 1.0f;
float         fadeTarget = 1.0f;
unsigned long fadeMs     = GALLERY_FADE_MS;

// Diagonal order (LED indices sorted by x+y), filled in setup().
uint8_t DIAG_ORDER[NUM_LEDS];
// Per-channel diagonal orders (DIAG_ORDER filtered to each region), in setup().
uint8_t CH1_ORDER[NUM_LEDS]; int CH1_LEN = 0;
uint8_t CH2_ORDER[NUM_LEDS]; int CH2_LEN = 0;

// --- MIDI envelopes ---
enum EnvPhase { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
struct EnvState { EnvPhase phase; unsigned long startMs; float startLevel, peak, level; };
EnvState env[NUM_LEDS];
int8_t   noteLed[MAX_TRACKS][128];   // [track][note] -> LED, -1 if not held
uint8_t  ledRef[NUM_LEDS];           // how many held notes map to this LED

// --- Random flash ---
struct Flash { uint8_t led; unsigned long startMs; bool active; };
Flash flashes[MAX_FLASHES];

// --- Gallery ---
GalPhase      galPhase    = GP_SHOW;
int           galIdx      = 0;
unsigned long galPatStart = 0;
unsigned long galLastPerf = 0;
bool          midiActive  = false;   // are we performing MIDI inside gallery?

// ============================ HELPERS =======================================
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float randf() { return random(10000) / 10000.0f; }
static inline int   trackIdx(int track) { int t = track % MAX_TRACKS; return t < 0 ? t + MAX_TRACKS : t; }
static float wrapMod(float x, float m) { float r = fmod(x, m); return r < 0.0f ? r + m : r; }

const char *sceneName(Scene s) {
  switch (s) {
    case SC_IDLE: return "IDLE"; case SC_SOLID: return "SOLID";
    case SC_COMPOSITE: return "COMPOSITE";
    case SC_FLASH: return "FLASH"; case SC_DOT_SEQ: return "DOT_SEQ";
    case SC_DOT_PAR: return "DOT_PARALLEL"; case SC_RANDOM: return "RANDOM_MORPH";
    case SC_MIDI_MAPPED: return "MIDI_MAPPED"; case SC_GALLERY: return "GALLERY";
  }
  return "?";
}

// ---- output ----
void setLed(int i, uint16_t duty12) {
  uint8_t ch = LED_CHANNEL[i];
  if ((int)ch == DISABLED_CHANNEL) duty12 = 0;
  boards[ch / 16].setPin(ch % 16, duty12, INVERT_PWM);
}

// 1.0 at the start of each line, ramping to DIM_FLOOR by its 60% point.
float dimGain(int i) {
  float pos, point, rampDist;
  if (i < 15) { pos = i;          point = TOP_60_POINT; rampDist = TOP_RAMP_DIST; }
  else        { pos = i - 15;     point = BOT_60_POINT; rampDist = BOT_RAMP_DIST; }
  if (rampDist <= 0) return (pos < point) ? 1.0f : DIM_FLOOR;
  float frac = clamp01((pos - (point - rampDist)) / rampDist);
  return 1.0f - frac * (1.0f - DIM_FLOOR);
}

void writeAll() {
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = clamp01(layer[i]) * dimGain(i);   // dim mask = master condition
    uint16_t duty = (uint16_t)(clamp01(v) * 4095.0f + 0.5f);
    setLed(i, duty);
  }
}

// ---- fade ----
void fadeTo(float target, unsigned long ms) { fadeTarget = target; fadeMs = ms ? ms : 1; }
void updateFade(float dtMs) {
  float step = dtMs / (float)fadeMs;
  if      (fadeLevel < fadeTarget) fadeLevel = min(fadeTarget, fadeLevel + step);
  else if (fadeLevel > fadeTarget) fadeLevel = max(fadeTarget, fadeLevel - step);
}
bool fadeIdle() { return fabs(fadeLevel - fadeTarget) < 0.001f; }

// ---- envelopes / MIDI ----
void clearMidi() {
  for (int i = 0; i < NUM_LEDS; i++) {
    env[i].phase = ENV_IDLE; env[i].level = env[i].peak = env[i].startLevel = 0; env[i].startMs = 0;
    ledRef[i] = 0;
  }
  for (int t = 0; t < MAX_TRACKS; t++) for (int n = 0; n < 128; n++) noteLed[t][n] = -1;
}

void updateEnvelope(int i, unsigned long now) {
  EnvState &e = env[i];
  unsigned long t = now - e.startMs;
  switch (e.phase) {
    case ENV_IDLE:    e.level = 0; break;
    case ENV_ATTACK:
      if (attackMs == 0 || t >= attackMs) { e.level = e.peak; e.phase = ENV_DECAY; e.startMs = now; }
      else e.level = e.startLevel + (e.peak - e.startLevel) * (t / (float)attackMs);
      break;
    case ENV_DECAY:
      if (decayMs == 0 || t >= decayMs) { e.level = e.peak * sustainLvl; e.phase = ENV_SUSTAIN; }
      else e.level = e.peak + (e.peak * sustainLvl - e.peak) * (t / (float)decayMs);
      break;
    case ENV_SUSTAIN: e.level = e.peak * sustainLvl; break;
    case ENV_RELEASE:
      if (releaseMs == 0 || t >= releaseMs) { e.level = 0; e.phase = ENV_IDLE; }
      else e.level = e.startLevel * (1.0f - t / (float)releaseMs);
      break;
  }
}

int pickRandomFreeLed() {
  int pool[NUM_LEDS], n = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    if ((int)LED_CHANNEL[i] == DISABLED_CHANNEL) continue;
    if (ledRef[i]) continue;
    pool[n++] = i;
  }
  return n ? pool[random(n)] : -1;
}

// Map a note to a LED for its channel.
//  - Bass (CH1_TRACK): DIRECT, one bass LED per semitone, wrapping (15..25).
//  - Chords (CH2_TRACK): diagonal spread of MAP_NOTE_LOW..HIGH across the region.
//  - Anything else: full-board diagonal spread.
int mapNoteToLed(int note, int track) {
  if (track == CH1_TRACK) {
    int off = (note - (int)BASS_NOTE_BASE) % (int)BASS_LED_COUNT;
    if (off < 0) off += (int)BASS_LED_COUNT;       // cyclic wrap on the bass humps
    return BASS_LED_LOW + off;
  }
  const uint8_t *order; int len;
  if (track == CH2_TRACK) { order = CH2_ORDER; len = CH2_LEN; }
  else                    { order = DIAG_ORDER; len = NUM_LEDS; }
  if (len <= 0) return -1;
  float f = clamp01((float)(note - MAP_NOTE_LOW) / (float)(MAP_NOTE_HIGH - MAP_NOTE_LOW));
  int rank = (int)(f * (len - 1) + 0.5f);
  return order[rank];
}

bool anyNotesHeld() {
  for (int t = 0; t < MAX_TRACKS; t++)
    for (int n = 0; n < 128; n++) if (noteLed[t][n] >= 0) return true;
  return false;
}

void perfNoteOn(int track, int note, int vel, bool mapped, unsigned long now) {
  int ti = trackIdx(track);
  int led;
  if (noteLed[ti][note] >= 0) led = noteLed[ti][note];   // retrigger same LED
  else led = mapped ? mapNoteToLed(note, track) : pickRandomFreeLed();
  if (led < 0) return;
  if (DEBUG_MIDI && mapped) {
    Serial.print(F("# MAP ch=")); Serial.print(track);
    Serial.print(F(" note="));    Serial.print(note);
    Serial.print(F(" -> LED "));  Serial.println(led);
  }
  if (noteLed[ti][note] < 0) { noteLed[ti][note] = (int8_t)led; ledRef[led]++; }
  env[led].peak       = constrain(vel, 0, 127) / 127.0f;
  env[led].startLevel = env[led].level;
  env[led].startMs    = now;
  env[led].phase      = ENV_ATTACK;
}

void perfNoteOff(int track, int note, unsigned long now) {
  int ti = trackIdx(track);
  int led = noteLed[ti][note];
  if (led < 0) return;
  noteLed[ti][note] = -1;
  if (ledRef[led] > 0 && --ledRef[led] == 0) {
    env[led].startLevel = env[led].level;
    env[led].startMs    = now;
    env[led].phase      = ENV_RELEASE;
  }
}

void allNotesOff(unsigned long now) {
  for (int t = 0; t < MAX_TRACKS; t++)
    for (int n = 0; n < 128; n++) if (noteLed[t][n] >= 0) perfNoteOff(t, n, now);
}

// ---- random flash ----
void resetFlashes() { for (int i = 0; i < MAX_FLASHES; i++) flashes[i].active = false; }

// ============================ SCENE RENDERERS ===============================
void buildMidiLayer(unsigned long now, float *out) {
  for (int i = 0; i < NUM_LEDS; i++) { updateEnvelope(i, now); out[i] = env[i].level; }
}

// `el` is the elapsed time within this pattern (ms); `now` is absolute (used by
// the stateful random flash). The COMPOSITE scene re-drives this with a local el.
void buildPatternLayer(Scene s, unsigned long el, unsigned long now, float morph, float *out) {
  for (int i = 0; i < NUM_LEDS; i++) out[i] = 0;
  switch (s) {
    case SC_COMPOSITE: {
      unsigned long total = 0;
      for (int k = 0; k < COMPOSITE_LEN; k++)
        total += (unsigned long)COMPOSITE_SEQ[k].loops * patternLoopMs(COMPOSITE_SEQ[k].pat);
      unsigned long cel = total ? (el % total) : 0;
      unsigned long acc = 0;
      for (int k = 0; k < COMPOSITE_LEN; k++) {
        unsigned long segDur = (unsigned long)COMPOSITE_SEQ[k].loops * patternLoopMs(COMPOSITE_SEQ[k].pat);
        if (cel < acc + segDur) { buildPatternLayer(COMPOSITE_SEQ[k].pat, cel - acc, now, 0.0f, out); return; }
        acc += segDur;
      }
      return;
    }
    case SC_SOLID:
      for (int i = 0; i < NUM_LEDS; i++) out[i] = 1.0f;
      break;
    case SC_FLASH: {
      bool on = (el % (FLASH_ON_MS + FLASH_OFF_MS)) < FLASH_ON_MS;
      for (int i = 0; i < NUM_LEDS; i++) out[i] = on ? 1.0f : 0.0f;
      break;
    }
    case SC_DOT_SEQ: {
      int step = (el / DOT_STEP_MS) % NUM_LEDS;
      out[DOT_SEQ_ORDER[step]] = 1.0f;
      break;
    }
    case SC_DOT_PAR: {
      int step = (el / DOT_STEP_MS) % 15;
      out[15 + step] = 1.0f;   // bottom line (Line 2)
      out[0  + step] = 1.0f;   // top line (Line 1)
      break;
    }
    case SC_RANDOM: {
      // spawn
      if (randf() < FLASH_RATE * (FRAME_MS / 1000.0f)) {
        for (int sct = 0; sct < MAX_FLASHES; sct++) {
          if (!flashes[sct].active) {
            flashes[sct].active = true; flashes[sct].startMs = now;
            flashes[sct].led = random(NUM_LEDS);
            break;
          }
        }
      }
      for (int sct = 0; sct < MAX_FLASHES; sct++) {
        if (!flashes[sct].active) continue;
        float age = (now - flashes[sct].startMs) / 1000.0f;
        float rigid = (age < RIGID_ON_S) ? 1.0f : 0.0f;
        float fade  = expf(-age * FADE_DECAY);
        float e = (1.0f - morph) * rigid + morph * fade;
        if (age > FLASH_LIFE_S || (e < 0.004f && age > RIGID_ON_S)) { flashes[sct].active = false; continue; }
        int led = flashes[sct].led;
        if (e > out[led]) out[led] = e;
      }
      break;
    }
    default: break;
  }
}

float morphFor(unsigned long now) {
  return clamp01((float)(now - sceneStart) / (float)RANDOM_MORPH_MS);
}

// ===================== GALLERY PATTERN BANK (from SozoCumulative) ===========
// The richer refined-patterns set. The gallery cycles these; each fills
// out[0..29] with [0,1] from a local time t (seconds since it became active).

void patternBlobGaussian(float t, float out[NUM_LEDS]) {
  const float CX = 2.0f, CY = 5.0f, SIGMA_MIN = 3.0f, SIGMA_MAX = 14.0f, BREATH_HZ = 0.22f;
  float u = sin(t * 2.0f * PI * BREATH_HZ) * 0.5f + 0.5f;
  float sigma = SIGMA_MIN + u * (SIGMA_MAX - SIGMA_MIN);
  float twoSigSq = 2.0f * sigma * sigma;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - CX, dy = YS[i] - CY;
    out[i] = exp(-(dx * dx + dy * dy) / twoSigSq);
  }
}
void patternPendulum(float t, float out[NUM_LEDS]) {
  const float SWING_HZ = 0.1f, SWING_AMP = 22.0f, SIGMA = 5.5f, CY = 5.0f;
  float cx = SWING_AMP * sin(t * 2.0f * PI * SWING_HZ);
  float twoSigSq = 2.0f * SIGMA * SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - cx, dy = YS[i] - CY;
    out[i] = exp(-(dx * dx + dy * dy) / twoSigSq);
  }
}
void patternTide(float t, float out[NUM_LEDS]) {
  const float TIDE_HZ = 0.1116f, X_HALF = 18.0f, BASE = 0.0072f, SWING = 0.5934f;
  float tilt = sin(t * 2.0f * PI * TIDE_HZ);
  for (int i = 0; i < NUM_LEDS; i++) {
    float xn = XS[i] / X_HALF;
    if (xn > 1.0f) xn = 1.0f; else if (xn < -1.0f) xn = -1.0f;
    out[i] = clamp01(BASE + 0.5f * SWING * (1.0f + tilt * xn));
  }
}
void patternSine(float t, float out[NUM_LEDS]) {
  const float K = 0.35f, OMEGA = 3.0f;
  for (int i = 0; i < NUM_LEDS; i++) out[i] = sin(K * XS[i] + OMEGA * t) * 0.5f + 0.5f;
}
void patternRampAcross(float t, float out[NUM_LEDS]) {
  const float X_MIN = -26.0f, X_MAX = 26.0f, RAMP_WIDTH = 22.0f, SWEEP_PERIOD = 5.0f;
  float u = wrapMod(t / SWEEP_PERIOD, 1.0f);
  float leadX = X_MIN - RAMP_WIDTH + u * (X_MAX - X_MIN + 2.0f * RAMP_WIDTH);
  for (int i = 0; i < NUM_LEDS; i++) {
    float d = leadX - XS[i];
    out[i] = (d >= 0.0f && d <= RAMP_WIDTH) ? (1.0f - d / RAMP_WIDTH) : 0.0f;
  }
}
void patternChase(float t, float out[NUM_LEDS]) {
  const float SPEED = 8.0f, TAIL = 5.0f;
  float pos = wrapMod(t * SPEED, (float)NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - pos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / TAIL;
    out[i] = f > 0.0f ? f : 0.0f;
  }
}
void patternTwoComets(float t, float out[NUM_LEDS]) {
  const int LINE_LEN = 15;
  const float SPEED_A = 9.0f, SPEED_B = 7.0f, TAIL = 4.0f;
  float posA = wrapMod(t * SPEED_A, (float)LINE_LEN);
  float posB = wrapMod(t * SPEED_B, (float)LINE_LEN);
  for (int i = 0; i < NUM_LEDS; i++) {
    int   j   = (i < LINE_LEN) ? i : (i - LINE_LEN);
    float pos = (i < LINE_LEN) ? posA : posB;
    float d   = fabs((float)j - pos);
    if (d > LINE_LEN / 2.0f) d = LINE_LEN - d;
    float f = 1.0f - d / TAIL;
    out[i] = f > 0.0f ? f : 0.0f;
  }
}
float scPos, scLastT, scModeUntil; int scMode;
void initStutterChase() { scPos = 0.0f; scLastT = 0.0f; scModeUntil = 0.0f; scMode = 0; }
void patternStutterChase(float t, float out[NUM_LEDS]) {
  const float SPEED_FWD = 9.0f, SPEED_REV = -7.0f, TAIL = 5.0f;
  float dt = t - scLastT;
  if (dt > 0.1f) dt = 0.033f; if (dt < 0.0f) dt = 0.0f;
  scLastT = t;
  if (t >= scModeUntil) {
    int r = random(100);
    scMode = (r < 65) ? 0 : (r < 88 ? 1 : 2);
    scModeUntil = t + 0.12f + (random(1000) / 1000.0f) * 0.55f;
  }
  float speed = (scMode == 0) ? SPEED_FWD : (scMode == 1 ? 0.0f : SPEED_REV);
  scPos += speed * dt;
  while (scPos < 0.0f) scPos += NUM_LEDS; while (scPos >= NUM_LEDS) scPos -= NUM_LEDS;
  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - scPos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / TAIL;
    out[i] = f > 0.0f ? f : 0.0f;
  }
}
void patternBumpCascade(float t, float out[NUM_LEDS]) {
  const int N_BUMPS = 6;
  const int STARTS[6] = { 0,  6, 11, 15, 20, 25 };
  const int ENDS[6]   = { 5, 10, 14, 19, 24, 29 };
  const int PEAKS[6]  = { 3,  8, 13, 19, 23, 27 };
  const float PULSE_DUR = 1.0f, STAGGER = 0.4f, REST = 1.0815f;
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
      float f = 1.0f - fabs((float)(i - PEAKS[p])) / span;
      if (f < 0.0f) f = 0.0f;
      float v = amp * f;
      if (v > out[i]) out[i] = v;
    }
  }
}
void patternRipple(float t, float out[NUM_LEDS]) {
  const float OX = 18.2f, OY = 10.66f, RING_W = 5.88f, RING_HZ = 0.5527f, RING_VEL = 19.74f, MAX_R = 72.0f;
  const int N_RINGS = 9;
  float ringPeriod = 1.0f / RING_HZ;
  float base = wrapMod(t, ringPeriod);
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY;
    float d = sqrt(dx * dx + dy * dy), bright = 0.0f;
    for (int k = 0; k < N_RINGS; k++) {
      float r = (base + k * ringPeriod) * RING_VEL;
      if (r > MAX_R) continue;
      float diff = fabs(d - r);
      if (diff > RING_W) continue;
      float v = (1.0f - diff / RING_W) * (1.0f - r / MAX_R);
      if (v > bright) bright = v;
    }
    out[i] = bright;
  }
}
void patternRecedeFill(float t, float out[NUM_LEDS]) {
  const float OX = 0.0f, OY = 0.0f, PERIOD = 10.8f, MAX_R = 50.0f, EDGE = 3.0f;
  float u = wrapMod(t, PERIOD) / PERIOD;
  float tri = (u < 0.5f) ? (u * 2.0f) : (2.0f - u * 2.0f);
  float r = tri * tri * (3.0f - 2.0f * tri) * MAX_R;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY;
    float d = sqrt(dx * dx + dy * dy);
    if (d <= r) out[i] = 1.0f;
    else if (d <= r + EDGE) out[i] = 1.0f - (d - r) / EDGE;
    else out[i] = 0.0f;
  }
}
void patternConvergence(float t, float out[NUM_LEDS]) {
  const float OX = 18.2f, OY = 10.66f, MAX_R = 46.0f, CYCLE = 4.0f, FLASH = 0.5f, RING_W = 4.0f;
  float u = wrapMod(t, CYCLE + FLASH);
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY;
    float d = sqrt(dx * dx + dy * dy), v = 0.0f;
    if (u < CYCLE) {
      float r = MAX_R * (1.0f - u / CYCLE), diff = fabs(d - r);
      if (diff < RING_W) v = 1.0f - diff / RING_W;
    } else {
      float center = 1.0f - (u - CYCLE) / FLASH;
      v = center * exp(-d * d / (2.0f * 4.0f * 4.0f));
    }
    out[i] = clamp01(v);
  }
}

struct GalPattern { const char *name; void (*fn)(float, float *); void (*init)(); };
const GalPattern GAL_PATTERNS[] = {
  { "blob_gaussian", patternBlobGaussian, NULL },
  { "pendulum",      patternPendulum,     NULL },
  { "tide",          patternTide,         NULL },
  { "sine_wave",     patternSine,         NULL },
  { "ramp_across",   patternRampAcross,   NULL },
  { "chase",         patternChase,        NULL },
  { "two_comets",    patternTwoComets,    NULL },
  { "stutter_chase", patternStutterChase, initStutterChase },
  { "bump_cascade",  patternBumpCascade,  NULL },
  { "ripple",        patternRipple,       NULL },
  { "recede_fill",   patternRecedeFill,   NULL },
  { "convergence",   patternConvergence,  NULL },
};
const int GAL_PATTERN_COUNT = sizeof(GAL_PATTERNS) / sizeof(GAL_PATTERNS[0]);

// ============================ GALLERY =======================================
// Advance to (re)start the current gallery pattern: reset its clock + init().
void galleryEnterPattern(unsigned long now) {
  galPatStart = now;
  if (GAL_PATTERNS[galIdx].init) GAL_PATTERNS[galIdx].init();
}

void enterGallery(unsigned long now) {
  galPhase    = GP_IN;
  galIdx      = 0;
  galLastPerf = now;
  midiActive  = false;
  galleryEnterPattern(now);
  fadeLevel = 0.0f;
  fadeTo(1.0f, GALLERY_FADE_MS);   // fade the first pattern up
}

void tickGallery(unsigned long now) {
  switch (galPhase) {
    case GP_SHOW:
      if (now - galPatStart >= GALLERY_HOLD_MS) { fadeTo(0.0f, GALLERY_FADE_MS); galPhase = GP_OUT_NEXT; }
      break;
    case GP_OUT_NEXT:
      if (fadeLevel <= 0.001f) {
        galIdx = (galIdx + 1) % GAL_PATTERN_COUNT;
        galleryEnterPattern(now);
        fadeTo(1.0f, GALLERY_FADE_MS); galPhase = GP_IN;
      }
      break;
    case GP_IN:
      if (fadeLevel >= 0.999f) { galPatStart = now; galPhase = GP_SHOW; }
      break;
    case GP_TO_MIDI:
      if (fadeLevel <= 0.001f) { midiActive = true; galPhase = GP_MIDI; }
      break;
    case GP_MIDI:
      if (!anyNotesHeld() && (now - galLastPerf) > MIDI_IDLE_MS) {
        midiActive = false;                                // drop MIDI overlay
        galleryEnterPattern(now);
        fadeTo(1.0f, GALLERY_FADE_MS); galPhase = GP_IN;   // pattern fades back in
      }
      break;
  }
}

// A performance note arrived while in gallery.
void galleryPerfNoteOn(int track, int note, int vel, unsigned long now) {
  galLastPerf = now;
  midiActive  = true;                                    // show notes immediately, overlaid on the fade-out
  perfNoteOn(track, note, vel, /*mapped=*/false, now);   // random assignment
  if (galPhase != GP_MIDI && galPhase != GP_TO_MIDI) {
    fadeTo(0.0f, GALLERY_FADE_MS);                // fade the pattern out, then perform
    galPhase = GP_TO_MIDI;
  }
}

// ============================ TOP-LEVEL RENDER ==============================
void renderActive(unsigned long now) {
  switch (scene) {
    case SC_IDLE:
      for (int i = 0; i < NUM_LEDS; i++) layer[i] = 0;
      break;
    case SC_SOLID: case SC_COMPOSITE: case SC_FLASH: case SC_DOT_SEQ: case SC_DOT_PAR: case SC_RANDOM:
      buildPatternLayer(scene, now - sceneStart, now, scene == SC_RANDOM ? morphFor(now) : 0.0f, layer);
      for (int i = 0; i < NUM_LEDS; i++) layer[i] *= fadeLevel;   // fadeLevel == 1 here
      break;
    case SC_MIDI_MAPPED:
      buildMidiLayer(now, layer);
      for (int i = 0; i < NUM_LEDS; i++) layer[i] *= fadeLevel;
      break;
    case SC_GALLERY: {
      buildMidiLayer(now, layerMidi);
      GAL_PATTERNS[galIdx].fn((now - galPatStart) / 1000.0f, layerPat);
      for (int i = 0; i < NUM_LEDS; i++) {
        float p = clamp01(layerPat[i]) * fadeLevel;
        float m = midiActive ? layerMidi[i] : 0.0f;
        layer[i] = p > m ? p : m;
      }
      break;
    }
  }
}

// ============================ SCENE SWITCH ==================================
void setScene(Scene s, unsigned long now) {
  scene = s; sceneStart = now;
  resetFlashes();
  clearMidi();
  fadeLevel = 1.0f; fadeTarget = 1.0f;    // performance scenes: instant, no fade
  if (s == SC_GALLERY) enterGallery(now);
  Serial.print(F("# SCENE -> ")); Serial.println(sceneName(s));
}

// note 3: become a full solid, fading up from black over SOLID_FADE_IN_MS.
void triggerSolidFadeIn(unsigned long now) {
  scene = SC_SOLID; sceneStart = now;
  resetFlashes(); clearMidi();
  fadeLevel = 0.0f; fadeTo(1.0f, SOLID_FADE_IN_MS);
  Serial.println(F("# SCENE -> SOLID (6s fade in)"));
}

// note 4: fade the whole board to black over BOARD_FADE_OUT_MS. The current
// scene keeps running underneath; the master fade just takes it to black.
void triggerFadeOut(unsigned long now) {
  fadeTo(0.0f, BOARD_FADE_OUT_MS);
  Serial.println(F("# FADE OUT (2s)"));
}

// ============================ MIDI / SERIAL =================================
void handleNoteOn(int track, int note, int vel, unsigned long now) {
  if (vel <= 0) { handleNoteOffByNote(track, note, now); return; }   // vel-0 = note off
  // Scene/control notes are matched on EVERY channel, always — `track` is
  // ignored here, so a scene change is never missed for being on the "wrong"
  // channel.
  switch (note) {
    case NOTE_SOLID:      setScene(SC_SOLID, now);       return;
    case NOTE_COMPOSITE:  setScene(SC_COMPOSITE, now);   return;
    case NOTE_RANDOM:     setScene(SC_RANDOM, now);      return;  // 2
    case NOTE_SOLID_FADE: triggerSolidFadeIn(now);       return;  // 3
    case NOTE_FADE_OUT:   triggerFadeOut(now);           return;  // 4
    case NOTE_MIDI:       setScene(SC_MIDI_MAPPED, now); return;
    case NOTE_GALLERY:    setScene(SC_GALLERY, now);     return;
    default: break;
  }
  // Notes 0..10 are reserved for control on all channels: even if not assigned a
  // scene yet, they are consumed here and never leak into performance.
  if (note <= CONTROL_NOTE_MAX) return;
  if (note >= PERF_NOTE_MIN) {                                 // performance note (channel-routed)
    if (scene == SC_MIDI_MAPPED)   perfNoteOn(track, note, vel, /*mapped=*/true, now);
    else if (scene == SC_GALLERY)  galleryPerfNoteOn(track, note, vel, now);
  }
}

void handleNoteOffByNote(int track, int note, unsigned long now) {
  if (note >= PERF_NOTE_MIN) {
    perfNoteOff(track, note, now);
    if (scene == SC_GALLERY) galLastPerf = now;
  }
}

// Forward decl resolved by Arduino auto-prototypes; defined above usage order-wise.

char buf[40];
int  bufLen = 0;

void handleLine(char *line, unsigned long now) {
  // trim trailing whitespace / CR
  int n = strlen(line);
  while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\r' || line[n-1] == '\t')) line[--n] = '\0';
  char *comma = strchr(line, ',');
  if (!comma) return;
  *comma = '\0';
  char *action = line;
  char *rest = comma + 1;

  if (strcmp(action, "start") == 0) {
    // new: start,tt,nn,vv   (fallback old: start,nn,vv -> track 0)
    char *c2 = strchr(rest, ',');
    if (!c2) return;
    char *c3 = strchr(c2 + 1, ',');
    if (c3) handleNoteOn(atoi(rest), atoi(c2 + 1), atoi(c3 + 1), now);
    else    handleNoteOn(0,          atoi(rest),   atoi(c2 + 1), now);
  } else if (strcmp(action, "stop") == 0) {
    // new: stop,tt,nn   (fallback old: stop,nn -> track 0)
    char *c2 = strchr(rest, ',');
    if (c2) handleNoteOffByNote(atoi(rest), atoi(c2 + 1), now);
    else    handleNoteOffByNote(0,          atoi(rest),   now);
  } else if (strcmp(action, "cc") == 0) {
    char *c2 = strchr(rest, ',');
    int num = atoi(rest);
    int val = c2 ? atoi(c2 + 1) : 0;
    if (num == 123 && val == 0) allNotesOff(now);   // all-notes-off / panic
    // (CC 20-23 ADSR hooks could go here later)
  }
}

void pollSerial(unsigned long now) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf[bufLen] = '\0';
      if (bufLen > 0) handleLine(buf, now);
      bufLen = 0;
    } else if (bufLen < (int)sizeof(buf) - 1) {
      buf[bufLen++] = c;
    } else {
      bufLen = 0;   // overflow -> resync
    }
  }
}

// ============================ SETUP / LOOP ==================================
void computeDiagOrder() {
  for (int i = 0; i < NUM_LEDS; i++) DIAG_ORDER[i] = i;
  for (int i = 1; i < NUM_LEDS; i++) {        // insertion sort by (x + y) ascending
    uint8_t key = DIAG_ORDER[i];
    float kv = XS[key] + YS[key];
    int j = i - 1;
    while (j >= 0 && (XS[DIAG_ORDER[j]] + YS[DIAG_ORDER[j]]) > kv) { DIAG_ORDER[j+1] = DIAG_ORDER[j]; j--; }
    DIAG_ORDER[j+1] = key;
  }
}

// Split the diagonal order into the two channel regions (preserving diagonal
// order). Channel 1 = LEDs 15..25; Channel 2 = the rest (0..14 and 26..29).
void buildChannelOrders() {
  CH1_LEN = CH2_LEN = 0;
  for (int r = 0; r < NUM_LEDS; r++) {
    int led = DIAG_ORDER[r];
    if (led >= 15 && led <= 25) CH1_ORDER[CH1_LEN++] = led;
    else                        CH2_ORDER[CH2_LEN++] = led;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  for (uint8_t b = 0; b < NUM_BOARDS; b++) {
    boards[b].begin();
    boards[b].setOscillatorFrequency(27000000);
    boards[b].setPWMFreq(PWM_FREQ_HZ);
  }
  Wire.setClock(400000);

  randomSeed(analogRead(A0));
  computeDiagOrder();
  buildChannelOrders();
  clearMidi();
  resetFlashes();

  for (int i = 0; i < NUM_LEDS; i++) layer[i] = 0;
  writeAll();                       // start dark
  scene = SC_IDLE; sceneStart = millis();
  lastFrameMs = millis();
  Serial.println(F("SozoUnveling ready. Waiting for scene triggers (notes 0-6)."));
}

void loop() {
  unsigned long now = millis();
  pollSerial(now);

  if (now - lastFrameMs >= FRAME_MS) {
    float dtMs = now - lastFrameMs;
    if (dtMs > 100.0f) dtMs = 100.0f;
    lastFrameMs = now;

    if (scene == SC_GALLERY) tickGallery(now);
    updateFade(dtMs);
    renderActive(now);
    writeAll();
  }
}
