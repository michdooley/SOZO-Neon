// SOZO — SozoUnveiling vintage: fade out. The board holds solid briefly, then
// fades to black over FADE_SEC and stays dark.
const int NUM_LEDS = 30;
const float HOLD_SEC = 1.0f;   // @knob 0 10 0.1  group:Speed "Hold before fade (s)"
const float FADE_SEC = 2.0f;   // @knob 0.2 20 0.1 group:Speed "Fade-out time (s)"
const float LEVEL    = 1.0f;   // @knob 0 1 0.01  group:Shape "Start brightness"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float g;
  if (t < HOLD_SEC) g = 1.0f;
  else { g = 1.0f - (t - HOLD_SEC) / FADE_SEC; if (g < 0.0f) g = 0.0f; }
  float v = LEVEL * g;
  for (int i = 0; i < NUM_LEDS; i++) {
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
