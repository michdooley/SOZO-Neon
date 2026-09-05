// SOZO LED visualizer — echo flash pattern.
//
// Random LEDs flash, then echo: each spawn produces N pulses at the
// same LED, evenly spaced and progressively dimmer — like a sound
// bouncing in a room. Multiple echo trains can overlap on different
// LEDs.

const int   NUM_LEDS    = 30;
const int   MAX_ECHOES  = 6;
const int   N_PULSES    = 3;
const float ECHO_DELAY  = 0.28f;
const float DIM_FACTOR  = 0.55f;   // each echo is this fraction of the previous
const float PULSE_DECAY = 12.0f;   // per-pulse decay (1/sec) — very fast
const float SPAWN_RATE  = 2.0f;    // echo trains spawned per second

struct Echo {
  int   led;
  float startT;
  bool  active;
};
Echo echoes[MAX_ECHOES];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_ECHOES; i++) echoes[i].active = false;
}

void loop() {
  float t = millis() / 1000.0f;

  if (random(1000) < (long)(SPAWN_RATE * 33.0f)) {
    for (int s = 0; s < MAX_ECHOES; s++) {
      if (!echoes[s].active) {
        echoes[s].led    = random(NUM_LEDS);
        echoes[s].startT = t;
        echoes[s].active = true;
        break;
      }
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;
    for (int s = 0; s < MAX_ECHOES; s++) {
      if (!echoes[s].active) continue;
      if (echoes[s].led != i) continue;
      for (int n = 0; n < N_PULSES; n++) {
        float age = t - echoes[s].startT - n * ECHO_DELAY;
        if (age < 0.0f) continue;
        float amp = 1.0f;
        for (int k = 0; k < n; k++) amp *= DIM_FACTOR;
        float v = amp * exp(-age * PULSE_DECAY);
        if (v > bright) bright = v;
      }
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  for (int s = 0; s < MAX_ECHOES; s++) {
    if (echoes[s].active && (t - echoes[s].startT) > N_PULSES * ECHO_DELAY + 0.5f) {
      echoes[s].active = false;
    }
  }

  delay(33);
}
