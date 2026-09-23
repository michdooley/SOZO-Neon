// SOZO LED visualizer — bouncing balls pattern.
//
// A handful of tight, tail-less dots bounce up and down each line — N balls per
// line, staggered and each at a slightly randomized speed so they never meet in
// the same place twice. The fall is a touch faster than the climb, so the dots
// have a little gravity. The two lines run at different base speeds (like
// two_comets, but with multiple balls and no tails).

const int   NUM_LEDS   = 30;
const int   LINE1_LEN  = 15;
const int   LINE2_LEN  = 15;

const int   N_BALLS    = 3;      // @knob 1 7 1      group:Counts "Balls per line"
const float SPEED_A    = 9.0f;   // @knob 0.5 25 0.1 group:Speed  "Line 1 speed (LEDs/s)"
const float SPEED_B    = 7.0f;   // @knob 0.5 25 0.1 group:Speed  "Line 2 speed (LEDs/s)"
const float SPEED_JIT  = 0.12f;  // @knob 0 0.4 0.01 group:Speed  "Speed randomness (±)"
const float DOWN_BOOST = 1.1f;   // @knob 1.0 1.6 0.01 group:Speed "Down-speed boost"
const float BALL_SIZE  = 1.6f;   // @knob 0.5 4 0.1  group:Shape  "Ball size (radius)"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t  = millis() / 1000.0f;
  float L1 = (float)(LINE1_LEN - 1);   // top index = distance of one leg (up or down)

  for (int i = 0; i < NUM_LEDS; i++) {
    int   j    = (i < LINE1_LEN) ? i : i - LINE1_LEN;   // local index on its line
    float s    = (i < LINE1_LEN) ? SPEED_A : SPEED_B;   // that line's base speed
    float seed = (i < LINE1_LEN) ? 0.0f : 11.0f;        // distinct jitter per line

    // Brightest of the N balls wins this LED (max, not sum) so dots stay tight
    // and don't blow out where two balls cross.
    float v = 0.0f;
    for (int k = 0; k < N_BALLS; k++) {
      // Stable per-ball pseudo-random in [0,1) — same every frame (no state),
      // but distinct per ball/line, so balls drift apart and don't sync up.
      float h  = sin(((float)k + 1.0f) * 12.9898f + seed) * 43758.5453f;
      h        = h - floor(h);
      float sk = s * (1.0f + (h - 0.5f) * 2.0f * SPEED_JIT);

      // Asymmetric bounce: climb at sk, fall a touch faster (sk * DOWN_BOOST).
      float upDur   = L1 / sk;
      float downDur = L1 / (sk * DOWN_BOOST);
      float period  = upDur + downDur;

      float tc  = fmod(t + (float)k / (float)N_BALLS * period, period);  // staggered start
      float pos = (tc < upDur) ? tc * sk
                               : L1 - (tc - upDur) * sk * DOWN_BOOST;

      float d = fabs((float)j - pos);
      float f = 1.0f - d / BALL_SIZE;   // symmetric, no tail
      if (f > v) v = f;
    }

    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);   // ~30 fps
}
