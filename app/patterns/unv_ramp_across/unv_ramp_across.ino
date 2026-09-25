// SOZO — SozoUnveiling bank: ramp sweeping across the board.
const int NUM_LEDS = 30;
const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};
const float X_MIN        = -26.0f;  // @knob -30 0 0.5  group:Shape "Left edge (in)"
const float X_MAX        =  26.0f;  // @knob 0 30 0.5   group:Shape "Right edge (in)"
const float RAMP_WIDTH   = 22.0f;   // @knob 4 40 0.5   group:Shape "Ramp width (in)"
const float SWEEP_PERIOD = 5.0f;    // @knob 0.5 20 0.1 group:Speed "Seconds per sweep"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float u = fmod(t / SWEEP_PERIOD, 1.0f);
  float leadX = X_MIN - RAMP_WIDTH + u * (X_MAX - X_MIN + 2.0f * RAMP_WIDTH);
  for (int i = 0; i < NUM_LEDS; i++) {
    float d = leadX - XS[i];
    float v = (d >= 0.0f && d <= RAMP_WIDTH) ? (1.0f - d / RAMP_WIDTH) : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
