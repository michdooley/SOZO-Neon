// SOZO LED visualizer — ripple pattern.
//
// Concentric rings expand outward from the shared node (idx 26), like
// dropping a stone in a pond. Several rings coexist at different radii;
// each fades as it spreads. Uses world distance so the rings are
// physically circular regardless of index order.

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

const float OX        = 18.20f;
const float OY        = 10.66f;
const float RING_W    = 3.5f;    // ring thickness (inches)
const float RING_HZ   = 0.55f;   // new ring spawned every 1/RING_HZ seconds
const float RING_VEL  = 14.0f;   // ring expansion speed (inches/sec)
const float MAX_R     = 50.0f;
const int   N_RINGS   = 4;       // simultaneous rings on screen

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float ringPeriod = 1.0f / RING_HZ;

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX;
    float dy = YS[i] - OY;
    float d  = sqrt(dx * dx + dy * dy);

    float bright = 0.0f;
    for (int k = 0; k < N_RINGS; k++) {
      float age = fmod(t, ringPeriod) + k * ringPeriod;
      float r   = age * RING_VEL;
      if (r > MAX_R) continue;
      float diff = fabs(d - r);
      if (diff > RING_W) continue;
      float v = (1.0f - diff / RING_W) * (1.0f - r / MAX_R);
      if (v > bright) bright = v;
    }

    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
