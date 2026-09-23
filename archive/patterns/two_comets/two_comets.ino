// SOZO LED visualizer — two comets racing pattern.
//
// One chase runs along line 1 (idx 0–14), another along line 2 (idx
// 15–29), at slightly different speeds. Each comet has a soft tail and
// wraps around when it reaches the end of its line. Because the speeds
// don't divide evenly, they drift in and out of sync — sometimes side
// by side, sometimes on opposite ends.

const int   NUM_LEDS  = 30;
const int   LINE1_LEN = 15;
const int   LINE2_LEN = 15;
const float SPEED_A   = 9.0f;   // line 1 comet speed (LEDs/sec)
const float SPEED_B   = 7.0f;   // line 2 comet speed
const float TAIL      = 4.0f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float posA = fmod(t * SPEED_A, (float)LINE1_LEN);
  float posB = fmod(t * SPEED_B, (float)LINE2_LEN);

  for (int i = 0; i < NUM_LEDS; i++) {
    float v;
    if (i < LINE1_LEN) {
      float d = fabs((float)i - posA);
      if (d > LINE1_LEN / 2.0f) d = LINE1_LEN - d;
      float f = 1.0f - d / TAIL;
      v = f > 0.0f ? f : 0.0f;
    } else {
      int j = i - LINE1_LEN;
      float d = fabs((float)j - posB);
      if (d > LINE2_LEN / 2.0f) d = LINE2_LEN - d;
      float f = 1.0f - d / TAIL;
      v = f > 0.0f ? f : 0.0f;
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
