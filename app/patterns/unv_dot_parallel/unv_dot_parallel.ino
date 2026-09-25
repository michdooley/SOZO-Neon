// SOZO — SozoUnveiling vintage: dot parallel. One tube on each line advances
// together (both lines in lock-step) — 15 steps per pass.
const int NUM_LEDS = 30;
const float STEP_SEC = 0.8f;   // @knob 0.05 3 0.05 group:Speed "Dot step (s)"
const float LEVEL    = 1.0f;   // @knob 0 1 0.01    group:Shape "Brightness"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  int step = floor(t / STEP_SEC);
  step = step % 15;
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = (i == step || i == 15 + step) ? LEVEL : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
