// SOZO LED visualizer — bump cascade pattern.
//
// Each of the 6 bumps (3 per line) lights as a soft pulse, staggered
// so the next bump begins ramping up while the previous one is still
// fading. Within each bump, the peak LED is brightest and the LEDs
// fade toward the bump's edges. Reads as a wave climbing across the
// piece, then a short rest, then the next cycle.

const int NUM_LEDS = 30;
const int N_BUMPS  = 6;

// Bump LED ranges and peak indices from pattern_guide.md.
const int STARTS[N_BUMPS] = { 0,  6, 11, 15, 20, 25 };
const int ENDS[N_BUMPS]   = { 5, 10, 14, 19, 24, 29 };
const int PEAKS[N_BUMPS]  = { 3,  8, 13, 19, 23, 27 };

const float PULSE_DUR = 1.0f;     // seconds each bump pulse takes
const float STAGGER   = 0.4f;     // seconds between consecutive bump starts
const float REST      = 0.7f;     // dark pause before the cycle restarts

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float cyclePeriod = (N_BUMPS - 1) * STAGGER + PULSE_DUR + REST;
  float cycleT = fmod(t, cyclePeriod);

  uint8_t buf[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) buf[i] = 0;

  for (int p = 0; p < N_BUMPS; p++) {
    float localT = cycleT - p * STAGGER;
    if (localT < 0.0f || localT > PULSE_DUR) continue;
    float amp = sin(localT / PULSE_DUR * PI);   // 0 → 1 → 0

    int rUp = PEAKS[p] - STARTS[p];
    int rDn = ENDS[p]  - PEAKS[p];
    int radius = (rUp > rDn) ? rUp : rDn;
    float span = (float)radius + 1.0f;

    for (int i = STARTS[p]; i <= ENDS[p]; i++) {
      float d = fabs((float)(i - PEAKS[p]));
      float falloff = 1.0f - d / span;
      if (falloff < 0.0f) falloff = 0.0f;
      int b = (int)(amp * falloff * 255.0f);
      if (b > buf[i]) buf[i] = b;
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(buf[i]);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
