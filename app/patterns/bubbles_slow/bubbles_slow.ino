// SOZO LED visualizer — bubbles (slow) pattern.
//
// One tube at a time swells like a bubble: brightness creeps up on a
// quadratic curve — barely moving at first, then rushing to full in the
// last moments — then it sits full and taut for a beat before popping to
// black, throwing a ring of light outward from where it burst. Bubbles overlap,
// so an old pop is still spreading while the next one is still growing.
// Nothing is born on the bottom tubes of either line (idx 0-1, 15-16) —
// the big low donuts only ever catch ripples.

const int NUM_LEDS  = 30;
const int LINE1_LEN = 15;   // idx 0-14 = line 1, 15-29 = line 2

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

// --- the three headline knobs -------------------------------------------
const float BUBBLE_GAP    = 2.20f;   // @knob 0.2 8 0.05 group:Overlap "Seconds between bubbles"
const float RAMP_TIME     = 3.50f;   // @knob 0.5 12 0.1 group:Speed "Bubble growth time (s)"
const float HOLD_TIME     = 0.35f;   // @knob 0 4 0.05 group:Speed "Hold at full before popping (s)"
const float RIPPLE_BRIGHT = 0.55f;   // @knob 0 1 0.01 group:Shape "Ripple brightness"

// --- shaping ------------------------------------------------------------
const float GROW_EXP      = 2.0f;    // @knob 1 6 0.1 group:Shape "Growth curve exponent (2 = quadratic)"
const float RIPPLE_VEL    = 22.0f;   // @knob 4 60 0.5 group:Speed "Ripple speed (in/s)"
const float RIPPLE_W      = 6.0f;    // @knob 1 16 0.5 group:Shape "Ripple thickness (in)"
const float RIPPLE_REACH  = 42.0f;   // @knob 10 75 1 group:Shape "Ripple fade-out radius (in)"
const int   MAX_BUBBLES   = 6;       // @knob 1 12 1 group:Counts "Max bubbles + ripples at once"
const int   SKIP_BOTTOM   = 2;       // @knob 0 6 1 group:Counts "Bottom tubes per line that can't bubble"

struct Bubble {
  int   led;
  float startT;
  bool  active;
};
Bubble bubbles[MAX_BUBBLES];
float nextSpawn = 0.0f;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int s = 0; s < MAX_BUBBLES; s++) bubbles[s].active = false;
  nextSpawn = 0.0f;
}

void loop() {
  float t = millis() / 1000.0f;
  float popT = RAMP_TIME + HOLD_TIME;   // bubble age at which it bursts

  // --- spawn a new bubble every BUBBLE_GAP seconds ----------------------
  if (t >= nextSpawn) {
    int slot = -1;
    for (int s = 0; s < MAX_BUBBLES; s++) {
      if (!bubbles[s].active) { slot = s; break; }
    }
    if (slot >= 0) {
      // Draw from the eligible tubes only: the first SKIP_BOTTOM of each
      // line are skipped, so k indexes the gap-free eligible set and is
      // mapped back onto a real LED index. Prefer a tube that isn't already
      // growing a bubble; give up after a few tries so a crowded board
      // still spawns something.
      int elig1 = LINE1_LEN - SKIP_BOTTOM;          // eligible tubes on line 1
      int nElig = NUM_LEDS - 2 * SKIP_BOTTOM;       // eligible tubes overall
      int pick  = SKIP_BOTTOM;
      for (int a = 0; a < 6; a++) {
        int k = random(nElig);
        pick = k < elig1 ? SKIP_BOTTOM + k : LINE1_LEN + SKIP_BOTTOM + (k - elig1);
        int busy = 0;
        for (int s = 0; s < MAX_BUBBLES; s++) {
          if (!bubbles[s].active) continue;
          if (bubbles[s].led != pick) continue;
          if (t - bubbles[s].startT < popT) busy = 1;
        }
        if (busy == 0) break;
      }
      bubbles[slot].led    = pick;
      bubbles[slot].startT = t;
      bubbles[slot].active = true;
    }
    nextSpawn = t + BUBBLE_GAP;
  }

  // --- render -----------------------------------------------------------
  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;

    for (int s = 0; s < MAX_BUBBLES; s++) {
      if (!bubbles[s].active) continue;
      float age = t - bubbles[s].startT;

      if (age < popT) {
        // Growing: quadratic swell, slow at first then racing to the peak,
        // then sitting at full volume for HOLD_TIME before it bursts.
        if (bubbles[s].led != i) continue;
        float v = 1.0f;
        if (age < RAMP_TIME) v = pow(age / RAMP_TIME, GROW_EXP);
        if (v > bright) bright = v;
      } else {
        // Popped: the source tube is black; a ring runs outward from it.
        if (bubbles[s].led == i) continue;
        float r  = (age - popT) * RIPPLE_VEL;
        if (r > RIPPLE_REACH) continue;
        float dx = XS[i] - XS[bubbles[s].led];
        float dy = YS[i] - YS[bubbles[s].led];
        float d  = sqrt(dx * dx + dy * dy);
        float diff = fabs(d - r);
        if (diff > RIPPLE_W) continue;
        float v = RIPPLE_BRIGHT * (1.0f - diff / RIPPLE_W) * (1.0f - r / RIPPLE_REACH);
        if (v > bright) bright = v;
      }
    }

    if (bright > 1.0f) bright = 1.0f;
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  // --- retire bubbles whose ripple has run off the board ----------------
  for (int s = 0; s < MAX_BUBBLES; s++) {
    if (!bubbles[s].active) continue;
    float age = t - bubbles[s].startT;
    if ((age - popT) * RIPPLE_VEL > RIPPLE_REACH) bubbles[s].active = false;
  }

  delay(33);
}
