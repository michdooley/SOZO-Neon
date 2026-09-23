// SOZO LED visualizer — pop and decay pattern.
//
// Long stretches of dark, then a sudden full-board burst that fades
// slowly back to black over a few seconds. The board blooms, then
// exhales. Recovery is the part you watch — the bursts themselves are
// sharp and quick, but the decay lingers.

const int   NUM_LEDS  = 30;
const int   MAX_POPS  = 4;
const float POP_RATE  = 0.35f;     // bursts per second (avg)
const float POP_RISE  = 0.04f;     // rise time to peak (s)
const float POP_DECAY = 0.55f;     // exponential decay rate (1/sec)

struct Pop {
  float startT;
  bool  active;
};
Pop pops[MAX_POPS];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_POPS; i++) pops[i].active = false;
}

void loop() {
  float t = millis() / 1000.0f;

  if (random(1000) < (long)(POP_RATE * 33.0f)) {
    for (int s = 0; s < MAX_POPS; s++) {
      if (!pops[s].active) {
        pops[s].startT = t;
        pops[s].active = true;
        break;
      }
    }
  }

  float vMax = 0.0f;
  for (int s = 0; s < MAX_POPS; s++) {
    if (!pops[s].active) continue;
    float age = t - pops[s].startT;
    float v;
    if (age < POP_RISE) {
      v = age / POP_RISE;
    } else {
      v = exp(-(age - POP_RISE) * POP_DECAY);
    }
    if (v > vMax) vMax = v;
    if (age > 8.0f) pops[s].active = false;
  }

  int b = (int)(vMax * 255.0f);
  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
