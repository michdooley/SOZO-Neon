// SOZO LED visualizer — random flash pattern.
//
// LEDs flash on briefly at random spots, then fade fast. Multiple
// overlapping flashes are allowed so the board feels like scattered
// sparks — quiet most of the time, the occasional pop. Each flash hits
// full brightness instantly and decays exponentially.

const int   NUM_LEDS    = 30;
const int   MAX_FLASHES = 8;
const float FLASH_DECAY = 4.5f;   // bigger = faster fade (1/sec)
const float FLASH_RATE  = 6.0f;   // average new flashes per second

struct Flash {
  int   led;
  float startT;
  bool  active;
};
Flash flashes[MAX_FLASHES];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_FLASHES; i++) flashes[i].active = false;
}

void loop() {
  float t = millis() / 1000.0f;

  // Spawn at most one flash per frame, with probability FLASH_RATE*frame_dt.
  // frame_dt ≈ 0.033s, so threshold = FLASH_RATE * 33 out of 1000.
  if (random(1000) < (long)(FLASH_RATE * 33.0f)) {
    for (int s = 0; s < MAX_FLASHES; s++) {
      if (!flashes[s].active) {
        flashes[s].led    = random(NUM_LEDS);
        flashes[s].startT = t;
        flashes[s].active = true;
        break;
      }
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;
    for (int s = 0; s < MAX_FLASHES; s++) {
      if (!flashes[s].active) continue;
      if (flashes[s].led != i) continue;
      float age = t - flashes[s].startT;
      float v   = exp(-age * FLASH_DECAY);
      if (v > bright) bright = v;
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  // Retire flashes once they've decayed below ~1/255 brightness.
  for (int s = 0; s < MAX_FLASHES; s++) {
    if (flashes[s].active && (t - flashes[s].startT) * FLASH_DECAY > 5.5f) {
      flashes[s].active = false;
    }
  }

  delay(33);
}
