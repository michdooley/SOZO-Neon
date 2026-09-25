// SOZO — SozoUnveiling vintage: flash (whole board on, then off). The most
// basic vintage cue — hard on/off, no easing.
const int NUM_LEDS = 30;
const float ON_SEC  = 2.0f;   // @knob 0.05 6 0.05 group:Speed "On time (s)"
const float OFF_SEC = 0.6f;   // @knob 0.05 6 0.05 group:Speed "Off time (s)"
const float LEVEL   = 1.0f;   // @knob 0 1 0.01    group:Shape "Brightness"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float ph = fmod(t, ON_SEC + OFF_SEC);
  float v = (ph < ON_SEC) ? LEVEL : 0.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
