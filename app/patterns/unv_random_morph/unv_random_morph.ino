// SOZO — SozoUnveiling vintage: random morph. Random tubes pop on; early in the
// cycle they read as rigid on/off squares, then over MORPH_SEC they soften into
// exponential fades. Stateful (a pool of active flashes carries across frames).
const int   NUM_LEDS    = 30;
const int   MAX_FLASHES = 16;
const float FLASH_RATE  = 7.0f;    // @knob 0.5 20 0.5 group:Spawn "Flashes per second"
const float RIGID_ON_S  = 0.18f;   // @knob 0.02 1 0.01 group:Shape "Rigid on-time (s)"
const float FADE_DECAY  = 3.0f;    // @knob 0.5 10 0.1  group:Shape "Fade decay (1/s)"
const float FLASH_LIFE_S = 2.5f;   // @knob 0.3 6 0.1   group:Shape "Max flash life (s)"
const float MORPH_SEC   = 15.0f;   // @knob 1 300 1     group:Morph "Rigid->fade time (s)"

struct Flash { int led; float startT; bool active; };
Flash flashes[MAX_FLASHES];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_FLASHES; i++) flashes[i].active = false;
}
void loop() {
  float t = millis() / 1000.0f;
  float morph = t / MORPH_SEC; if (morph > 1.0f) morph = 1.0f;

  if (random(1000) < (long)(FLASH_RATE * 33.0f)) {
    for (int s = 0; s < MAX_FLASHES; s++) {
      if (!flashes[s].active) {
        flashes[s].led = random(NUM_LEDS);
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
      float rigid = (age < RIGID_ON_S) ? 1.0f : 0.0f;
      float fade  = exp(-age * FADE_DECAY);
      float e = (1.0f - morph) * rigid + morph * fade;
      if (e > bright) bright = e;
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }

  for (int s = 0; s < MAX_FLASHES; s++) {
    if (flashes[s].active && (t - flashes[s].startT) > FLASH_LIFE_S) flashes[s].active = false;
  }
  delay(33);
}
