// SOZO — SozoUnveiling bank: bump cascade (6 staggered soft pulses).
const int NUM_LEDS = 30;
const int N_BUMPS  = 6;
const int STARTS[N_BUMPS] = { 0,  6, 11, 15, 20, 25 };
const int ENDS[N_BUMPS]   = { 5, 10, 14, 19, 24, 29 };
const int PEAKS[N_BUMPS]  = { 3,  8, 13, 19, 23, 27 };
const float PULSE_DUR = 1.0f;    // @knob 0.2 3 0.05  group:Speed "Pulse length (s)"
const float STAGGER   = 0.4f;    // @knob 0.05 2 0.01 group:Speed "Stagger between bumps (s)"
const float REST      = 1.0815f; // @knob 0 4 0.05    group:Speed "Rest before restart (s)"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float cyclePeriod = (N_BUMPS - 1) * STAGGER + PULSE_DUR + REST;
  float cycleT = fmod(t, cyclePeriod);
  uint8_t buf[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) buf[i] = 0;
  for (int p = 0; p < N_BUMPS; p++) {
    float localT = cycleT - p * STAGGER;
    if (localT < 0.0f || localT > PULSE_DUR) continue;
    float amp = sin(localT / PULSE_DUR * PI);
    int rUp = PEAKS[p] - STARTS[p];
    int rDn = ENDS[p]  - PEAKS[p];
    int radius = (rUp > rDn) ? rUp : rDn;
    float span = (float)radius + 1.0f;
    for (int i = STARTS[p]; i <= ENDS[p]; i++) {
      float f = 1.0f - fabs((float)(i - PEAKS[p])) / span;
      if (f < 0.0f) f = 0.0f;
      int bb = (int)(amp * f * 255.0f);
      if (bb > buf[i]) buf[i] = bb;
    }
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(buf[i]); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
