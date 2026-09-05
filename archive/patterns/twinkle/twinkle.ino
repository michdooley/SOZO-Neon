// SOZO LED visualizer — twinkle pattern.
//
// Every LED gently breathes with its own slow sinusoid, offset in phase
// and rate so the overall texture feels like a random shimmer instead
// of a coherent wave. Quiet and ambient — the piece looks alive without
// any clear motion. Good as an idle/screensaver pattern.

const int NUM_LEDS = 30;

// Per-LED phase offsets (radians) and rates (Hz). Hand-picked to feel
// random while not sharing common factors, so the overall pattern
// doesn't visibly loop.
const float PHASES[NUM_LEDS] = {
  0.00f, 1.23f, 2.71f, 0.85f, 3.92f, 1.61f, 4.40f, 2.05f, 0.31f, 5.18f,
  3.04f, 1.87f, 4.71f, 0.99f, 2.45f, 5.66f, 1.10f, 3.50f, 0.55f, 4.20f,
  2.30f, 5.05f, 1.45f, 3.80f, 0.71f, 2.90f, 4.85f, 1.95f, 3.30f, 0.40f
};
const float RATES[NUM_LEDS] = {
  0.27f, 0.41f, 0.33f, 0.55f, 0.22f, 0.48f, 0.36f, 0.29f, 0.45f, 0.31f,
  0.50f, 0.24f, 0.38f, 0.43f, 0.26f, 0.52f, 0.34f, 0.28f, 0.46f, 0.39f,
  0.32f, 0.49f, 0.25f, 0.42f, 0.37f, 0.51f, 0.30f, 0.44f, 0.35f, 0.47f
};

const float BASE  = 0.15f;   // minimum brightness floor (0..1)
const float SWING = 0.85f;   // peak swing above the base

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float u = sin(t * 2.0f * PI * RATES[i] + PHASES[i]) * 0.5f + 0.5f;
    float v = BASE + u * SWING;
    if (v > 1.0f) v = 1.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
