// SOZO LED visualizer — tide pattern.
//
// The whole board "sloshes" left and right like water tilting in a
// basin: one side is bright while the other is dim, and the bright
// side smoothly transfers across the board on a slow cycle. Tilt is
// applied to a brightness gradient in world x.

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};

const float TIDE_HZ = 0.225f;
const float X_HALF  = 25.0f;     // normalize x to roughly [-1, 1]
const float BASE    = 0.08f;
const float SWING   = 0.92f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float tilt = sin(t * 2.0f * PI * TIDE_HZ);

  for (int i = 0; i < NUM_LEDS; i++) {
    float xn = XS[i] / X_HALF;
    if (xn > 1.0f)  xn = 1.0f;
    if (xn < -1.0f) xn = -1.0f;
    float v = BASE + 0.5f * SWING * (1.0f + tilt * xn);
    if (v > 1.0f) v = 1.0f;
    if (v < 0.0f) v = 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
