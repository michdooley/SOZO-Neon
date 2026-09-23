// SOZO LED visualizer — lightning pattern.
//
// Rare full-board strikes: a quick multi-spike flicker, then a slow
// afterglow that fades over a couple of seconds. The board is mostly
// dark; the surprise of the strike is the point. The flicker during
// the spike uses a pseudo-random per-strike seed so each strike has
// its own jagged signature.

const int   NUM_LEDS    = 30;
const int   MAX_STRIKES = 3;
const float STRIKE_RATE = 0.25f;   // strikes per second (avg)
const float SPIKE_DUR   = 0.35f;   // total spike-flicker duration (s)
const float AFTER_DECAY = 1.4f;    // afterglow decay rate (1/sec)

struct Strike {
  float startT;
  unsigned long seed;
  bool active;
};
Strike strikes[MAX_STRIKES];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_STRIKES; i++) strikes[i].active = false;
}

void loop() {
  float t = millis() / 1000.0f;

  if (random(10000) < (long)(STRIKE_RATE * 330.0f)) {
    for (int s = 0; s < MAX_STRIKES; s++) {
      if (!strikes[s].active) {
        strikes[s].startT = t;
        strikes[s].seed   = random(1, 100000);
        strikes[s].active = true;
        break;
      }
    }
  }

  float vMax = 0.0f;
  for (int s = 0; s < MAX_STRIKES; s++) {
    if (!strikes[s].active) continue;
    float age = t - strikes[s].startT;
    float v;
    if (age < SPIKE_DUR) {
      // ~60 Hz flicker driven by a per-strike pseudo-random hash.
      unsigned long phase = (unsigned long)(age * 60.0f);
      unsigned long h = strikes[s].seed * 1103515245UL + phase * 12345UL;
      v = ((h >> 16) & 0xFF) / 255.0f;
      if (v < 0.4f) v = 0.4f;   // never fully dark mid-spike
    } else {
      v = exp(-(age - SPIKE_DUR) * AFTER_DECAY);
    }
    if (v > vMax) vMax = v;
    if (age > 5.0f) strikes[s].active = false;
  }

  int b = (int)(vMax * 255.0f);
  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
