// SOZO LED visualizer — rolling sine wave pattern.
//
// brightness = sin(K * world_x + OMEGA * t), projected onto each LED's
// actual x-coordinate. A sinusoidal brightness wave rolls across the
// board, riding the LEDs' real positions instead of the strip-index
// order. K sets the spatial wavelength; OMEGA sets scroll speed and
// direction (positive = travels right → left).

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};

const float K     = 0.35f;       // radians per inch
const float OMEGA = 3.0f;        // radians per second

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float phase = K * XS[i] + OMEGA * t;
    float v = sin(phase) * 0.5f + 0.5f;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
