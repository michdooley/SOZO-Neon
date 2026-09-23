// SOZO LED visualizer — convergence pattern.
//
// A bright ring contracts from the outer edges of the board inward to
// the shared node (idx 26), where it collapses into a brief gaussian
// flash before the cycle restarts. Reads as light "flowing in" from
// the far corners and gathering at the meeting point of the two lines.

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};
const float YS[NUM_LEDS] = {
   -9.52f, -2.56f,  3.06f,  5.97f,  4.54f,  1.21f,  5.94f, 10.24f, 12.83f, 12.26f,
    9.21f, 13.34f, 16.27f, 16.84f, 14.94f,
  -14.01f, -7.86f, -2.64f,  0.63f,  0.93f, -1.96f,  3.13f,  7.04f,  9.20f,  8.89f,
    6.81f, 10.66f, 13.49f, 14.37f, 13.00f
};

const float OX     = 18.20f;     // shared node x (idx 26)
const float OY     = 10.66f;
const float MAX_R  = 46.0f;
const float CYCLE  = 4.0f;       // seconds for the contraction
const float FLASH  = 0.5f;       // hold + decay at the center
const float RING_W = 4.0f;       // ring thickness

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float u = fmod(t, CYCLE + FLASH);

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX;
    float dy = YS[i] - OY;
    float d  = sqrt(dx * dx + dy * dy);

    float v = 0.0f;
    if (u < CYCLE) {
      float r = MAX_R * (1.0f - u / CYCLE);
      float diff = fabs(d - r);
      if (diff < RING_W) v = 1.0f - diff / RING_W;
    } else {
      float fu = (u - CYCLE) / FLASH;
      float center = 1.0f - fu;
      v = center * exp(-d * d / (2.0f * 4.0f * 4.0f));
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
