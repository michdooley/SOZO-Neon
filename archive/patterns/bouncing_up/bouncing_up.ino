// SOZO LED visualizer — bouncing up pattern.
//
// Tail-less dots stream endlessly upward on each line: every ball climbs from
// the bottom, slips off the top, and a fresh one rises from the bottom — a
// steady upward ladder, N balls per line at slightly randomized speeds. Like
// bouncing_balls, but the dots never fall back down.

const int   NUM_LEDS   = 30;
const int   LINE1_LEN  = 15;
const int   LINE2_LEN  = 15;

const int   N_BALLS    = 3;      // @knob 1 7 1      group:Counts "Balls per line"
const float SPEED_A    = 9.0f;   // @knob 0.5 25 0.1 group:Speed  "Line 1 speed (LEDs/s)"
const float SPEED_B    = 7.0f;   // @knob 0.5 25 0.1 group:Speed  "Line 2 speed (LEDs/s)"
const float SPEED_JIT  = 0.12f;  // @knob 0 0.4 0.01 group:Speed  "Speed randomness (±)"
const float BALL_SIZE  = 1.6f;   // @knob 0.5 4 0.1  group:Shape  "Ball size (radius)"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t    = millis() / 1000.0f;
  float span = (float)LINE1_LEN;   // climb 0..L-1, then wrap and reappear at bottom

  for (int i = 0; i < NUM_LEDS; i++) {
    int   j    = (i < LINE1_LEN) ? i : i - LINE1_LEN;   // local index on its line
    float s    = (i < LINE1_LEN) ? SPEED_A : SPEED_B;   // that line's base speed
    float seed = (i < LINE1_LEN) ? 0.0f : 11.0f;        // distinct jitter per line

    // Brightest of the N balls wins this LED (max, not sum) so dots stay tight
    // and don't blow out where two balls cross.
    float v = 0.0f;
    for (int k = 0; k < N_BALLS; k++) {
      // Stable per-ball pseudo-random in [0,1) — same every frame (no state),
      // but distinct per ball/line, so balls drift apart instead of marching
      // in lockstep.
      float h  = sin(((float)k + 1.0f) * 12.9898f + seed) * 43758.5453f;
      h        = h - floor(h);
      float sk = s * (1.0f + (h - 0.5f) * 2.0f * SPEED_JIT);

      float offset = (float)k / (float)N_BALLS * span;   // even initial spacing
      float pos    = fmod(t * sk + offset, span);        // marches up, wraps to bottom
      float d      = fabs((float)j - pos);
      float f      = 1.0f - d / BALL_SIZE;               // symmetric, no tail
      if (f > v) v = f;
    }

    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);   // ~30 fps
}
