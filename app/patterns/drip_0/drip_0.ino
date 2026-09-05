// SOZO LED visualizer — Drip_0: drips running down the mountains.
//
// A drip is born bright at the center of a peak, then falls under gravity —
// slow at first, accelerating as it goes. Because it lights whichever LEDs sit
// at its current height near that peak, and a peak has a slope on each side, the
// glow runs down BOTH sides at once like water poured on a summit. Each drip
// fades as it falls and simply vanishes once it drops below the trough — it does
// not bounce. Drips are spawned at random times on random peaks, so the piece
// keeps trickling.
//
// The fall is modeled in world height (YS). A drip at height h lights LED i by
//   gx = horizontal nearness of the LED to the peak's x   (keeps it on this peak)
//   gy = vertical  nearness of the LED to the falling height h  (the drip band)
// so the lit set is a short glowing band that descends the two slopes.

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

// The peaks drips are born at (local Y-maxima of each mountain ridge), in world
// coords. First three are on line 1, last two on line 2.
const int   N_PEAKS = 5;
const float PEAK_X[N_PEAKS] = { -12.63f,  1.35f, 13.25f, 13.32f, 21.75f };
const float PEAK_Y[N_PEAKS] = {   5.97f, 12.83f, 16.84f,  9.20f, 14.37f };

const int   MAX_DRIPS = 6;
const float DRIP_RATE = 0.6f;   // @knob 0.05 4 0.05 group:Timing "Drips per second"

const float V0        = 0.8f;   // @knob 0 6 0.1    group:Fall "Initial speed (in/s)"
const float GRAVITY   = 3.0f;   // @knob 0.5 14 0.5 group:Fall "Gravity (accel)"
const float MAX_DROP  = 13.0f;  // @knob 4 26 0.5   group:Fall "Fall distance before it vanishes"

const float BAND_W    = 2.0f;   // @knob 0.5 6 0.1  group:Look "Drip height (band width)"
const float X_SPREAD  = 7.0f;   // @knob 2 14 0.5   group:Look "Side spread (down the slopes)"
const float FADE      = 0.35f;  // @knob 0 2 0.05   group:Look "Fade rate (per sec)"

struct Drip {
  float px;       // the peak this drip is running down
  float py;
  float startT;
  bool  active;
};
Drip drips[MAX_DRIPS];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_DRIPS; i++) drips[i].active = false;
}

void loop() {
  float t = millis() / 1000.0f;

  // Spawn at most one drip per frame, at a random peak (frame_dt ~= 0.033s).
  if (random(1000) < (long)(DRIP_RATE * 33.0f)) {
    for (int s = 0; s < MAX_DRIPS; s++) {
      if (!drips[s].active) {
        int p = random(N_PEAKS);
        drips[s].px     = PEAK_X[p];
        drips[s].py     = PEAK_Y[p];
        drips[s].startT = t;
        drips[s].active = true;
        break;
      }
    }
  }

  float twoBand = 2.0f * BAND_W * BAND_W;
  float twoSpread = 2.0f * X_SPREAD * X_SPREAD;

  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;
    for (int s = 0; s < MAX_DRIPS; s++) {
      if (!drips[s].active) continue;
      float age  = t - drips[s].startT;
      float drop = V0 * age + 0.5f * GRAVITY * age * age;   // accelerating fall
      float h    = drips[s].py - drop;                      // current height

      float dxv = XS[i] - drips[s].px;
      float dyv = YS[i] - h;
      float gx = exp(-(dxv * dxv) / twoSpread);   // near this peak's column
      float gy = exp(-(dyv * dyv) / twoBand);     // near the falling band

      // Taper to nothing as it approaches the trough so it vanishes (no pop),
      // and an exponential time-fade on top of that.
      float taper = 1.0f - drop / MAX_DROP;
      if (taper < 0.0f) taper = 0.0f;
      taper = taper * taper;

      float v = gx * gy * taper * exp(-age * FADE);
      if (v > bright) bright = v;
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  // Retire a drip once it has fallen past the trough (below MAX_DROP).
  for (int s = 0; s < MAX_DRIPS; s++) {
    if (!drips[s].active) continue;
    float age  = t - drips[s].startT;
    float drop = V0 * age + 0.5f * GRAVITY * age * age;
    if (drop > MAX_DROP) drips[s].active = false;
  }

  delay(33);
}
