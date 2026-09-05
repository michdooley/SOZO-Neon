// SOZO LED visualizer — blob (sphere phasing through) pattern.
//
// A 3D sphere drifts across the board while bobbing through the wall
// plane. We render its cross-section: a disc that fades in, grows,
// peaks when the sphere center is on the plane, then shrinks and fades
// out — like an orb phasing through. The whole sphere also translates
// left-to-right, so it appears, blooms, vanishes, reappears further
// along, etc.
//
// Uses world (x,y) coordinates so the motion is spatial, not in
// strip-index order.

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

const float SPHERE_R   = 8.0f;     // sphere radius (inches)
const float TRAVEL_S   = 9.0f;     // seconds for one full pass across
const float PHASE_HZ   = 0.55f;    // through-plane oscillation rate
const float X_MIN      = -28.0f;
const float X_MAX      =  28.0f;
const float Y_BOB_AMP  = 4.0f;
const float Y_BOB_HZ   = 0.27f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;

  float u  = fmod(t / TRAVEL_S, 1.0f);
  float cx = X_MIN + u * (X_MAX - X_MIN);
  float cy = sin(t * 2.0f * PI * Y_BOB_HZ) * Y_BOB_AMP + 2.0f;
  float cz = sin(t * 2.0f * PI * PHASE_HZ) * (SPHERE_R * 1.1f);

  // Disc radius where the sphere cuts the plane.
  float crossR2 = SPHERE_R * SPHERE_R - cz * cz;
  if (crossR2 < 0.0f) crossR2 = 0.0f;
  float crossR  = sqrt(crossR2);

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - cx;
    float dy = YS[i] - cy;
    float d  = sqrt(dx * dx + dy * dy);

    float v = 0.0f;
    if (crossR >= 0.5f) {
      float core = crossR * 0.5f;
      if (d <= core) {
        v = 1.0f;
      } else if (d < crossR) {
        v = 1.0f - (d - core) / (crossR - core);
      }
    }

    int b = (int)(v * 255.0f);
    if (b < 0)   b = 0;
    if (b > 255) b = 255;
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
