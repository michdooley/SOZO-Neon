// SOZO — SozoUnveiling bank: single chase comet that wraps all 30 tubes.
const int NUM_LEDS = 30;
const float SPEED = 8.0f;   // @knob 0.5 25 0.1 group:Speed "Comet speed (tubes/s)"
const float TAIL  = 5.0f;   // @knob 1 12 0.5   group:Shape "Tail length (tubes)"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float pos = fmod(t * SPEED, (float)NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - pos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / TAIL;
    float v = f > 0.0f ? f : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
