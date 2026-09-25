// SOZO — SozoUnveiling bank: ripple rings from the shared node.
const int NUM_LEDS = 30;
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
const float OX      = 18.20f;   // @knob -25 25 0.1  group:Position "Origin X"
const float OY      = 10.66f;   // @knob -15 20 0.1  group:Position "Origin Y"
const float RING_W  = 5.88f;    // @knob 1 12 0.1    group:Shape "Ring thickness (in)"
const float RING_HZ = 0.5527f;  // @knob 0.05 3 0.01 group:Speed "Rings per second"
const float RING_VEL= 19.74f;   // @knob 2 40 0.1    group:Speed "Ring speed (in/s)"
const float MAX_R   = 72.0f;    // @knob 20 100 1    group:Shape "Max radius (in)"
const int   N_RINGS = 9;        // @knob 1 12 1      group:Counts "Rings on screen"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float ringPeriod = 1.0f / RING_HZ;
  float base = fmod(t, ringPeriod);
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
    int b = (int)(bright * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
