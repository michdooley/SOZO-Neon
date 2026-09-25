// SOZO — SozoUnveiling vintage: solid (whole board on).
const int NUM_LEDS = 30;
const float LEVEL = 1.0f;   // @knob 0 1 0.01 group:Shape "Brightness"
void setup() { Serial.begin(115200); }
void loop() {
  for (int i = 0; i < NUM_LEDS; i++) {
    int b = (int)(LEVEL * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
