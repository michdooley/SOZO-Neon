// SOZO — SozoUnveiling vintage: fade in. The board ramps from black up to a
// full solid over FADE_SEC, then holds.
const int NUM_LEDS = 30;
const float FADE_SEC = 6.0f;   // @knob 0.2 20 0.1 group:Speed "Fade-in time (s)"
const float LEVEL    = 1.0f;   // @knob 0 1 0.01   group:Shape "Target brightness"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float g = t / FADE_SEC;
  if (g > 1.0f) g = 1.0f;
  float v = LEVEL * g;
  for (int i = 0; i < NUM_LEDS; i++) {
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
