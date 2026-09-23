// SOZO LED visualizer — rain pattern.
//
// Bright droplets spawn at the top of the board and fall under gravity,
// streaking down past the bumps with accelerating velocity. Each drop
// is a small gaussian; multiple drops can be in flight at once. Drops
// despawn when they fall off the bottom.

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

const int   MAX_DROPS  = 8;
const float SPAWN_RATE = 3.0f;     // drops per second (average)
const float G_ACC      = 35.0f;    // gravity (inches/sec^2)
const float DROP_SIGMA = 1.6f;
const float X_LO       = -20.0f;
const float X_HI       =  22.0f;
const float Y_TOP      =  22.0f;
const float Y_BOT      = -20.0f;

struct Drop {
  float x;
  float y;
  float vy;
  bool  active;
};
Drop drops[MAX_DROPS];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_DROPS; i++) drops[i].active = false;
}

void loop() {
  const float dt = 0.033f;

  if (random(1000) < (long)(SPAWN_RATE * 33.0f)) {
    for (int s = 0; s < MAX_DROPS; s++) {
      if (!drops[s].active) {
        drops[s].x  = X_LO + (random(1000) / 1000.0f) * (X_HI - X_LO);
        drops[s].y  = Y_TOP;
        drops[s].vy = -2.0f;
        drops[s].active = true;
        break;
      }
    }
  }

  for (int s = 0; s < MAX_DROPS; s++) {
    if (!drops[s].active) continue;
    drops[s].vy -= G_ACC * dt;
    drops[s].y  += drops[s].vy * dt;
    if (drops[s].y < Y_BOT) drops[s].active = false;
  }

  float twoSigSq = 2.0f * DROP_SIGMA * DROP_SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;
    for (int s = 0; s < MAX_DROPS; s++) {
      if (!drops[s].active) continue;
      float dx = XS[i] - drops[s].x;
      float dy = YS[i] - drops[s].y;
      float d2 = dx * dx + dy * dy;
      float g  = exp(-d2 / twoSigSq);
      if (g > bright) bright = g;
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
