// SOZO LED visualizer — bubbles (fast, proper spawner) pattern.
//
// The bubbles_slow idea run as a particle field: tubes swell on a quadratic
// curve, hold, pop to black and throw a ripple outward. Here the spawner is
// a real arrival process — exponential gaps around a mean rate, a per-bubble
// growth time so no two are clones, and a pool deep enough that a new bubble
// never has to wait for an old one to die. Turn the rate up and it stays an
// even scatter instead of breaking into waves.

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

// --- spawn process ------------------------------------------------------
const float SPAWN_GAP    = 0.35f;   // @knob 0.05 3 0.01 group:Overlap "Mean seconds between bubbles"
const float SPAWN_JITTER = 0.70f;   // @knob 0 1 0.01 group:Overlap "Spawn randomness (0 = metronome, 1 = full)"

// --- bubble life --------------------------------------------------------
const float RAMP_TIME     = 1.10f;   // @knob 0.1 6 0.05 group:Speed "Mean bubble growth time (s)"
const float RAMP_JITTER   = 0.35f;   // @knob 0 0.9 0.01 group:Shape "Growth-time spread between bubbles"
const float HOLD_TIME     = 0.10f;   // @knob 0 2 0.01 group:Speed "Hold at full before popping (s)"
const float GROW_EXP      = 2.0f;    // @knob 1 6 0.1 group:Shape "Growth curve exponent (2 = quadratic)"

// --- pop ripple ---------------------------------------------------------
const float RIPPLE_BRIGHT = 0.45f;   // @knob 0 1 0.01 group:Shape "Ripple brightness"
const float RIPPLE_VEL    = 26.0f;   // @knob 4 60 0.5 group:Speed "Ripple speed (in/s)"
const float RIPPLE_W      = 5.0f;    // @knob 1 16 0.5 group:Shape "Ripple thickness (in)"
const float RIPPLE_REACH  = 34.0f;   // @knob 10 75 1 group:Shape "Ripple fade-out radius (in)"

// --- pool + placement ---------------------------------------------------
const int   MAX_BUBBLES   = 16;      // @knob 2 24 1 group:Counts "Bubble + ripple pool size"
const int   SKIP_BOTTOM   = 2;       // @knob 0 6 1 group:Counts "Bottom tubes per line that can't bubble"

struct Bubble {
  int   led;
  float startT;
  float ramp;      // this bubble's own growth time
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

  // --- spawn ------------------------------------------------------------
  // An arrival process, not a slot poller: each bubble schedules the next
  // one from its own due time (not from `t`), so the long-run rate is
  // exactly SPAWN_GAP no matter what the pool is doing. Up to 4 arrivals
  // can land in one frame, which is what keeps fast rates from silently
  // collapsing into bursts.
  for (int a = 0; a < 4; a++) {
    if (t < nextSpawn) break;

    // Prefer a free slot; otherwise recycle the oldest bubble (its ripple
    // is the faintest and furthest out). Never make a spawn wait on a
    // death — that coupling is what turns a fast rate into waves.
    int slot = -1;
    float oldest = -1.0f;
    int oldestSlot = 0;
    for (int s = 0; s < MAX_BUBBLES; s++) {
      if (!bubbles[s].active) { slot = s; break; }
      float sAge = t - bubbles[s].startT;
      if (sAge > oldest) { oldest = sAge; oldestSlot = s; }
    }
    if (slot < 0) slot = oldestSlot;

    // Pick from the eligible tubes only (the first SKIP_BOTTOM of each line
    // never bubble), retrying a few times to dodge a tube that's already
    // swelling.
    int elig1 = LINE1_LEN - SKIP_BOTTOM;
    int nElig = NUM_LEDS - 2 * SKIP_BOTTOM;
    int pick  = SKIP_BOTTOM;
    for (int c = 0; c < 4; c++) {
      int k = random(nElig);
      pick = k < elig1 ? SKIP_BOTTOM + k : LINE1_LEN + SKIP_BOTTOM + (k - elig1);
      int busy = 0;
      for (int s = 0; s < MAX_BUBBLES; s++) {
        if (!bubbles[s].active) continue;
        if (bubbles[s].led != pick) continue;
        if (t - bubbles[s].startT < bubbles[s].ramp + HOLD_TIME) busy = 1;
      }
      if (busy == 0) break;
    }

    // Own growth time, so lifetimes decorrelate and pops never fall in step.
    float ru = (random(2001) - 1000) * 0.001f;          // -1 .. +1
    float rt = RAMP_TIME * (1.0f + RAMP_JITTER * ru);
    if (rt < 0.05f) rt = 0.05f;

    bubbles[slot].led    = pick;
    bubbles[slot].startT = t;
    bubbles[slot].ramp   = rt;
    bubbles[slot].active = true;

    // Next arrival: blend metronome with an exponential (Poisson) draw. The
    // mean of -log(u) is 1, so the average gap stays SPAWN_GAP at any
    // jitter — the knob changes the texture, not the rate.
    float u = (random(10000) + 1) * 0.0001f;            // (0, 1]
    float gap = SPAWN_GAP * ((1.0f - SPAWN_JITTER) + SPAWN_JITTER * (-log(u)));
    if (gap > SPAWN_GAP * 4.0f) gap = SPAWN_GAP * 4.0f;  // no dead lulls
    nextSpawn = nextSpawn + gap;
  }
  if (nextSpawn < t) nextSpawn = t;   // don't hoard debt if the rate outruns the frame

  // --- render -----------------------------------------------------------
  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;

    for (int s = 0; s < MAX_BUBBLES; s++) {
      if (!bubbles[s].active) continue;
      float age  = t - bubbles[s].startT;
      float popT = bubbles[s].ramp + HOLD_TIME;

      if (age < popT) {
        // Growing on a quadratic swell, then full and taut before bursting.
        if (bubbles[s].led != i) continue;
        float v = 1.0f;
        if (age < bubbles[s].ramp) v = pow(age / bubbles[s].ramp, GROW_EXP);
        if (v > bright) bright = v;
      } else {
        // Popped: the source tube is black; a ring runs outward from it.
        if (bubbles[s].led == i) continue;
        float r = (age - popT) * RIPPLE_VEL;
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
    if ((age - bubbles[s].ramp - HOLD_TIME) * RIPPLE_VEL > RIPPLE_REACH) bubbles[s].active = false;
  }

  delay(33);
}
