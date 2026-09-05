// SOZO LED visualizer — lub-dub heartbeat pattern.
//
// Two pulses close together (lub-dub), then a long quiet pause before
// the next beat. The whole board pulses in unison, like a single muscle
// contracting. Calmer and more organic than a single-thump heartbeat —
// you actually feel the resting interval between beats.

const int NUM_LEDS = 30;

const float BPM       = 56.0f;       // beats per minute
const float DUB_DELAY = 0.18f;       // s between lub and dub
const float PULSE_DUR = 0.16f;       // each pulse's rise + fall
const float DUB_SCALE = 0.70f;       // dub is softer than lub

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float cycle = 60.0f / BPM;
  float u = fmod(t, cycle);

  float vLub = 0.0f;
  float vDub = 0.0f;
  if (u < PULSE_DUR) vLub = sin(u / PULSE_DUR * PI);
  float u2 = u - DUB_DELAY;
  if (u2 > 0.0f && u2 < PULSE_DUR) vDub = DUB_SCALE * sin(u2 / PULSE_DUR * PI);

  float v = vLub > vDub ? vLub : vDub;
  int b = (int)(v * 255.0f);

  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
