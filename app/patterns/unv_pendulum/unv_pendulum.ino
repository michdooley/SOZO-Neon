// SOZO — SozoUnveiling bank: pendulum (gaussian blob swinging in x).
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
const float SWING_HZ  = 0.1f;    // @knob 0.02 1 0.01 group:Speed "Swing rate (Hz)"
const float SWING_AMP = 22.0f;   // @knob 2 26 0.5    group:Shape "Swing amplitude (in)"
const float SIGMA     = 5.5f;    // @knob 1 15 0.5    group:Shape "Blob radius"
const float CY        = 5.0f;    // @knob -15 20 0.5  group:Position "Center Y"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float cx = SWING_AMP * sin(t * 2.0f * PI * SWING_HZ);
  float twoSigSq = 2.0f * SIGMA * SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - cx, dy = YS[i] - CY;
    float v = exp(-(dx * dx + dy * dy) / twoSigSq);
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
