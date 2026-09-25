// SOZO — SozoUnveiling vintage: dot sequential. One tube at a time walks the
// bottom line (15..29) then the top line (0..14) — a vintage marquee chase.
const int NUM_LEDS = 30;
const int ORDER[NUM_LEDS] = {
  15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
};
const float STEP_SEC = 0.8f;   // @knob 0.05 3 0.05 group:Speed "Dot step (s)"
const float LEVEL    = 1.0f;   // @knob 0 1 0.01    group:Shape "Brightness"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  int step = floor(t / STEP_SEC);
  step = step % NUM_LEDS;
  int lit = ORDER[step];
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = (i == lit) ? LEVEL : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
