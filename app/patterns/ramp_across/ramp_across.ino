// SOZO LED visualizer — ramp across pattern.
//
// A linear brightness ramp sweeps horizontally across the board: bright
// at the leading edge, fading off behind it. The ramp enters from the
// left, crosses the whole width, exits on the right, then resets. Uses
// world x-coordinates so the ramp travels spatially, not in strip-index
// order.

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};

const float X_MIN        = -26.0f;
const float X_MAX        =  26.0f;
const float RAMP_WIDTH   = 22.0f;   // inches behind the leading edge
const float SWEEP_PERIOD = 5.0f;    // seconds per left-to-right pass

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float u = fmod(t / SWEEP_PERIOD, 1.0f);
  float leadX = X_MIN - RAMP_WIDTH + u * (X_MAX - X_MIN + 2.0f * RAMP_WIDTH);

  for (int i = 0; i < NUM_LEDS; i++) {
    float d = leadX - XS[i];        // distance behind the leading edge
    float v = 0.0f;
    if (d >= 0.0f && d <= RAMP_WIDTH) {
      v = 1.0f - d / RAMP_WIDTH;    // brightest at the edge, fading back
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
