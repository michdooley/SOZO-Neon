// SOZO LED visualizer — chase test v2 pattern.
//
// A red comet sweeps the top line end-to-end, then (after a pause) the bottom
// line sweeps the same way — one row at a time, never both at once — with a
// tunable gap between rows and a tunable hold before the cycle repeats.

const int   NUM_LEDS    = 30;
const int   LINE1_LEN   = 15;     // LEDs per line (structural; both lines are 15)

const float CHASE_SPEED = 8.0f;   // @knob 0.5 25 0.1 group:Speed "Speed (LEDs/s)"
const float CHASE_TAIL  = 5.0f;   // @knob 1 12 0.5   group:Shape "Tail length"
const float ROW_GAP     = 0.2f;   // @knob 0 5 0.05   group:Speed "Row 1→2 delay (s)"
const float REPEAT_GAP  = 0.5f;   // @knob 0 8 0.1    group:Speed "Delay before repeat (s)"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;

  // One row's comet enters from before LED 0 (head at -TAIL) and exits past the
  // last LED (head at (LEN-1)+TAIL), so the row builds from dark and returns to
  // dark within a single sweep.
  float sweepDist = (LINE1_LEN - 1) + 2.0f * CHASE_TAIL;
  float sweepDur  = sweepDist / CHASE_SPEED;

  // Timeline per cycle: row1 sweep | ROW_GAP | row2 sweep | REPEAT_GAP.
  float cycle     = 2.0f * sweepDur + ROW_GAP + REPEAT_GAP;
  float tc        = fmod(t, cycle);

  float row1Start = 0.0f;
  float row2Start = sweepDur + ROW_GAP;

  for (int i = 0; i < NUM_LEDS; i++) {
    int   local = (i < LINE1_LEN) ? i : i - LINE1_LEN;
    float start = (i < LINE1_LEN) ? row1Start : row2Start;

    float pt = tc - start;            // seconds into this row's own sweep
    float v  = 0.0f;
    if (pt >= 0.0f && pt < sweepDur) {
      float pos     = -CHASE_TAIL + pt * CHASE_SPEED;   // comet head, local index
      float d       = fabs((float)local - pos);
      float falloff = 1.0f - d / CHASE_TAIL;
      v = (falloff > 0.0f) ? falloff : 0.0f;
    }

    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);   // ~30 fps
}
