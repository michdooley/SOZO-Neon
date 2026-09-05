// Chase — a comet sweeps the whole strip with a soft tail. Linear
// brightness falloff over `tail` LEDs. Same logic as chase_test.ino.

export default {
  id: 'chase',
  name: 'Chase',
  params: {
    speed: { type: 'float', min: 0.5, max: 25, default: 8,  step: 0.1, label: 'Speed (LEDs/s)' },
    tail:  { type: 'float', min: 1,   max: 15, default: 5,  step: 0.1, label: 'Tail length' },
  },

  render(t, p, out) {
    const N = 30;
    const pos = ((t * p.speed) % N + N) % N;
    for (let i = 0; i < N; i++) {
      let d = Math.abs(i - pos);
      if (d > N / 2) d = N - d;
      const f = 1 - d / p.tail;
      out[i] = f > 0 ? Math.floor(f * 255) : 0;
    }
  },

  toArduino(p) {
    return `// Generated from chase.js
const int   NUM_LEDS    = 30;
const float CHASE_SPEED = ${p.speed.toFixed(3)}f;
const float CHASE_TAIL  = ${p.tail.toFixed(3)}f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float pos = fmod(t * CHASE_SPEED, (float)NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - pos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / CHASE_TAIL;
    int b = (f > 0.0f) ? (int)(f * 255.0f) : 0;
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\\n');
  }
  delay(33);
}
`;
  },
};
