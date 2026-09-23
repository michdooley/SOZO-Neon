// SOZO LED visualizer — resonance pattern.
//
// Like twinkle, but each LED's oscillation rate depends on its world
// x-position: LEDs on the left swing slowly, LEDs on the right swing
// fast. Over time the differing rates produce sweeping interference
// fronts and beats that visibly emerge from what initially looks like
// noise.

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};

const float HZ_LO  = 0.18f;
const float HZ_HI  = 1.20f;
const float X_HALF = 25.0f;
const float BASE   = 0.10f;
const float SWING  = 0.90f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float u = (XS[i] / X_HALF + 1.0f) * 0.5f;   // 0..1 across the board
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    float rate = HZ_LO + u * (HZ_HI - HZ_LO);
    float s = sin(t * 2.0f * PI * rate) * 0.5f + 0.5f;
    float v = BASE + s * SWING;
    if (v > 1.0f) v = 1.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
