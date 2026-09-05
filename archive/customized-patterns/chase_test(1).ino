// SOZO LED visualizer — chase test pattern.
//
// A red comet travels through all 30 donuts with a soft tail.
// Distinct from the visualizer's idle flash (which lights all donuts in unison),
// so if you see the comet in the visualizer you know serial is fully wired up.
//
// Protocol: 30 brightness ints (0-255), comma-separated, newline-terminated, 115200 baud.
//   idx 0-14  → line 1 in path order (15 LEDs)
//   idx 15-29 → line 2 in path order (15 LEDs; shared node lives at idx 26)

const int   NUM_LEDS    = 30;
const float CHASE_SPEED = 8.0f;   // LEDs traversed per second
const float CHASE_TAIL  = 5.0f;   // brightness falloff distance in indices

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t   = millis() / 1000.0f;
  float pos = fmod(t * CHASE_SPEED, (float)NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - pos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;        // wrap-around distance
    float falloff = 1.0f - d / CHASE_TAIL;
    int   b       = (falloff > 0.0f) ? (int)(falloff * 255.0f) : 0;

    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);   // ~30 fps
}
