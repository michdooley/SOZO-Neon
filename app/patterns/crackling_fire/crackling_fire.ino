// SOZO LED visualizer — crackling fire pattern.
//
// The fire sits in the lower-left and burns out along a diagonal: heat is a
// function of distance along the bottom-left/top-right axis, so the edge of
// the blaze is a diagonal front rather than a horizontal line. Flame character
// is a separate, vertical field — low tubes are coals that pulse near full,
// and the higher a tube sits in y the more it licks in and out through a gamma
// curve. Logs crack in the hot core, flashing a tube white and throwing an
// ember that climbs its line toward the upper right, burning out on the way.
// Only the embers carry state; the flicker is three rolling sines.

const int NUM_LEDS  = 30;
const int LINE1_LEN = 15;

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

// Per-tube phase offsets, so neighbours don't flicker in lockstep.
const float PHASES[NUM_LEDS] = {
  0.587f, 0.270f, 0.717f, 0.513f, 0.897f, 0.105f, 0.734f, 0.228f, 0.298f, 0.921f,
  0.127f, 0.564f, 0.039f, 0.006f, 0.907f, 0.218f, 0.687f, 0.165f, 0.990f, 0.641f,
  0.846f, 0.931f, 0.579f, 0.097f, 0.396f, 0.625f, 0.728f, 0.665f, 0.124f, 0.588f
};

// The hot core, in order of heat — where logs crack and embers are born.
const int BED_LEDS[8] = { 0, 15, 1, 16, 2, 3, 5, 17 };

// --- where the fire is (diagonal) ---------------------------------------
// diag = x*AXIS_X + y*AXIS_Y, in inches along the bottom-left/top-right axis.
// Full heat up to FIRE_CORE, dying out by FIRE_EDGE.
const float AXIS_X     = 0.60f;   // @knob -1 1 0.05 group:Position "Fire axis, x weight"
const float AXIS_Y     = 0.80f;   // @knob -1 1 0.05 group:Position "Fire axis, y weight"
const float FIRE_CORE  = -13.0f;  // @knob -22 12 0.5 group:Position "Full-heat edge (in along axis)"
const float FIRE_EDGE  = 3.00f;   // @knob -20 26 0.5 group:Position "Heat dies out (in along axis)"
const float FIRE_FALL  = 1.00f;   // @knob 0.4 4 0.05 group:Shape "Heat falloff across the front"
const float BED_LVL    = 0.78f;   // @knob 0 1 0.01 group:Shape "Coal brightness"

// --- flame character (vertical) -----------------------------------------
// Coals below COAL_TOP just pulse; the higher a tube sits, the more it runs
// through the gamma curve, which spends most of its time near zero and spikes
// — the difference between a glow that wobbles and a flame that licks.
const float COAL_TOP   = -6.00f;  // @knob -14 10 0.5 group:Position "Coal top (y, in)"
const float FLAME_H    = 14.0f;   // @knob 2 30 0.5 group:Position "Flame height above coals (in)"
const float AMP_BED    = 0.18f;   // @knob 0 1 0.01 group:Shape "Coal pulse depth"
const float FLAME_GAM  = 2.60f;   // @knob 1 6 0.05 group:Shape "Flame spikiness (higher = more gaps)"
const float FLAME_GAIN = 2.60f;   // @knob 0.5 5 0.05 group:Shape "Flame lick brightness"
const float CRACKLE_HZ = 1.00f;   // @knob 0.2 6 0.05 group:Speed "Flicker speed"
const float ROLL_K     = 0.28f;   // @knob 0 1.5 0.01 group:Shape "Upward roll (per inch)"
const float BREATHE    = 0.10f;   // @knob 0 0.5 0.01 group:Shape "Slow breathing depth"

// --- cracks + embers (travel up-and-right along the lines) --------------
const float EMBER_GAP  = 1.80f;   // @knob 0.2 12 0.1 group:Overlap "Mean seconds between cracks"
const float EMBER_VEL  = 3.50f;   // @knob 0.5 12 0.1 group:Speed "Ember rise speed (tubes/s)"
const float EMBER_RISE = 18.0f;   // @knob 1 22 0.5 group:Shape "Ember travel before burnout (tubes)"
const float EMBER_LVL  = 1.00f;   // @knob 0 1 0.01 group:Shape "Ember brightness"
const float EMBER_W    = 1.20f;   // @knob 0.6 3 0.05 group:Shape "Ember width (tubes)"
const float CRACK_LVL  = 1.00f;   // @knob 0 1 0.01 group:Shape "Crack flash brightness"
const float CRACK_T    = 0.12f;   // @knob 0.02 0.6 0.01 group:Speed "Crack flash length (s)"
const int   MAX_EMBERS = 5;       // @knob 1 10 1 group:Counts "Max embers at once"
const int   N_BED      = 4;       // @knob 1 8 1 group:Counts "Core tubes that can throw embers"

struct Ember {
  int   led;
  float startT;
  bool  active;
};
Ember embers[MAX_EMBERS];
float nextEmber = 0.0f;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int s = 0; s < MAX_EMBERS; s++) embers[s].active = false;
  nextEmber = 0.0f;
}

void loop() {
  float t = millis() / 1000.0f;

  // --- a log cracks: flash a core tube and launch an ember --------------
  for (int a = 0; a < 3; a++) {
    if (t < nextEmber) break;

    int slot = -1;
    float oldest = -1.0f;
    int oldestSlot = 0;
    for (int s = 0; s < MAX_EMBERS; s++) {
      if (!embers[s].active) { slot = s; break; }
      float sAge = t - embers[s].startT;
      if (sAge > oldest) { oldest = sAge; oldestSlot = s; }
    }
    if (slot < 0) slot = oldestSlot;

    embers[slot].led    = BED_LEDS[random(N_BED)];
    embers[slot].startT = t;
    embers[slot].active = true;

    float u   = (random(10000) + 1) * 0.0001f;
    float gap = EMBER_GAP * (0.35f + 0.65f * (-log(u)));
    if (gap > EMBER_GAP * 3.0f) gap = EMBER_GAP * 3.0f;
    nextEmber = nextEmber + gap;
  }
  if (nextEmber < t) nextEmber = t;

  // --- render -----------------------------------------------------------
  float breathe = 1.0f + BREATHE * sin(t * 0.9f);

  for (int i = 0; i < NUM_LEDS; i++) {
    float x = XS[i];
    float y = YS[i];

    // How much fire reaches here: measured along the diagonal axis.
    float diag = x * AXIS_X + y * AXIS_Y;
    float heat = (FIRE_EDGE - diag) / (FIRE_EDGE - FIRE_CORE);
    if (heat < 0.0f) heat = 0.0f;
    if (heat > 1.0f) heat = 1.0f;
    float body = pow(heat, FIRE_FALL) * BED_LVL;

    // How flame-like it is here: purely vertical, so flames stand up.
    float lick = (y - COAL_TOP) / FLAME_H;
    if (lick < 0.0f) lick = 0.0f;
    if (lick > 1.0f) lick = 1.0f;

    // Three sines that never come back into phase, rolling upward in y.
    float ph = PHASES[i] * 6.2832f;
    float n  = 0.50f * sin(t * CRACKLE_HZ *  3.1f - y * ROLL_K        + ph)
             + 0.32f * sin(t * CRACKLE_HZ *  5.7f - y * ROLL_K * 1.7f + ph * 2.3f + 1.3f)
             + 0.18f * sin(t * CRACKLE_HZ * 11.3f - y * ROLL_K * 2.9f + ph * 3.7f + 2.7f);

    // Gentle pulse in the coals, spiky lick at the tips, crossfaded by height.
    float m    = 0.5f + 0.5f * n;
    float bedM = 1.0f + AMP_BED * n;
    float tipM = pow(m, FLAME_GAM) * FLAME_GAIN;
    float v    = body * breathe * (bedM + (tipM - bedM) * lick);
    if (v < 0.0f) v = 0.0f;

    for (int s = 0; s < MAX_EMBERS; s++) {
      if (!embers[s].active) continue;
      float age = t - embers[s].startT;
      int   src = embers[s].led;

      // The crack itself — a hard flash on the tube that popped.
      if (i == src && age < CRACK_T) {
        float cv = CRACK_LVL * (1.0f - age / CRACK_T);
        if (cv > v) v = cv;
      }

      // Embers ride their own line, which runs bottom-left to upper-right.
      if (src < LINE1_LEN && i >= LINE1_LEN) continue;
      if (src >= LINE1_LEN && i < LINE1_LEN) continue;

      float travel = age * EMBER_VEL;
      float fade   = 1.0f - travel / EMBER_RISE;
      if (fade <= 0.0f) continue;
      float d = fabs((float)i - ((float)src + travel));
      if (d >= EMBER_W) continue;
      float ev = EMBER_LVL * (1.0f - d / EMBER_W) * fade;
      if (ev > v) v = ev;
    }

    if (v > 1.0f) v = 1.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  // --- burn out embers that have gone as far as they go ------------------
  for (int s = 0; s < MAX_EMBERS; s++) {
    if (!embers[s].active) continue;
    if ((t - embers[s].startT) * EMBER_VEL > EMBER_RISE) embers[s].active = false;
  }

  delay(33);
}
