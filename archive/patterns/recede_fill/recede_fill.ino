// SOZO LED visualizer — recede / fill pattern.
//
// Light fills the board outward from a central origin (the shared node
// at idx 26) until the whole piece is lit, then recedes back inward to
// the origin. Soft edge on the wavefront so the transition feels
// liquid. Eased timing so it accelerates into the middle and decelerates
// at the extremes.
//
// Uses world distance from the origin so the fill is radial, not
// strip-ordered.

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

const float OX     = 18.20f;   // origin (shared node, idx 26)
const float OY     = 10.66f;
const float PERIOD = 6.0f;     // seconds per fill+recede cycle
const float MAX_R  = 50.0f;    // covers the diagonal of the board
const float EDGE   = 3.0f;     // wavefront soft-edge width (inches)

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float u   = fmod(t, PERIOD) / PERIOD;
  float tri = (u < 0.5f) ? (u * 2.0f) : (2.0f - u * 2.0f);   // 0 → 1 → 0
  float eased = tri * tri * (3.0f - 2.0f * tri);             // smoothstep
  float r = eased * MAX_R;

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX;
    float dy = YS[i] - OY;
    float d  = sqrt(dx * dx + dy * dy);

    float v;
    if (d <= r) {
      v = 1.0f;
    } else if (d <= r + EDGE) {
      v = 1.0f - (d - r) / EDGE;
    } else {
      v = 0.0f;
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
