// SOZO LED visualizer — divergence pattern.
//
// A single bright wavefront emanates outward from the shared node
// (idx 26) — like a sonar ping — with a soft trailing fade behind the
// leading edge. Each pulse expands until it exits the board, then a
// new one starts at the center.

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

const float OX    = 18.20f;
const float OY    = 10.66f;
const float MAX_R = 50.0f;
const float CYCLE = 3.0f;
const float TAIL  = 10.0f;       // trailing fade length (inches)

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float u = fmod(t, CYCLE) / CYCLE;
  float r = u * MAX_R;

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX;
    float dy = YS[i] - OY;
    float d  = sqrt(dx * dx + dy * dy);

    float v = 0.0f;
    if (d <= r && d >= r - TAIL) {
      v = 1.0f - (r - d) / TAIL;
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
