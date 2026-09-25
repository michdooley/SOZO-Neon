// SOZO — SozoUnveiling bank: two comets, one per line, drifting in/out of sync.
const int   NUM_LEDS = 30;
const int   LINE_LEN = 15;
const float SPEED_A  = 9.0f;   // @knob 0.5 25 0.1 group:Speed "Line 1 speed (tubes/s)"
const float SPEED_B  = 7.0f;   // @knob 0.5 25 0.1 group:Speed "Line 2 speed (tubes/s)"
const float TAIL     = 4.0f;   // @knob 1 10 0.1   group:Shape "Tail length (tubes)"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float posA = fmod(t * SPEED_A, (float)LINE_LEN);
  float posB = fmod(t * SPEED_B, (float)LINE_LEN);
  for (int i = 0; i < NUM_LEDS; i++) {
    int   j   = (i < LINE_LEN) ? i : (i - LINE_LEN);
    float pos = (i < LINE_LEN) ? posA : posB;
    float d   = fabs((float)j - pos);
    if (d > LINE_LEN / 2.0f) d = LINE_LEN - d;
    float f = 1.0f - d / TAIL;
    float v = f > 0.0f ? f : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
