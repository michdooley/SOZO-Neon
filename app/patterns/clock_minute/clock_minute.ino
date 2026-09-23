// SOZO LED visualizer — clock: the whole board is one hour.
//
// One hand, both lines, sixty minutes. Line 1 carries :00 to :30 and line 2
// carries :30 to :00, so the hand sweeps the top line for the first half hour,
// drops to the bottom line on the half, and sweeps again — which line is lit
// tells you which half of the hour you're in. Each line is 12 tube-steps
// (the top 2 tubes stay dark), so a step is 2.5 minutes and the interpolated
// hand reads to about a minute. Dim ticks sit on the quarters: the ends and
// middle of each line are :00, :15, :30, :45.

const int NUM_LEDS  = 30;
const int LINE1_LEN = 15;

const float TIME_SCALE  = 1.0f;      // @knob 1 3600 1 group:Speed "Time scale (1 = real time)"
const float TIME_OFFSET = 2220.0f;   // @knob 0 3600 10 group:Position "Start time (s past the hour)"

const int   SKIP_TOP  = 2;        // @knob 0 4 1 group:Counts "Top tubes per line left dark"
const float TICK_LVL  = 0.10f;    // @knob 0 0.5 0.01 group:Shape "Quarter tick brightness"

const float HAND_W    = 1.00f;    // @knob 0.6 4 0.05 group:Shape "Hand width (tubes)"
const float HAND_LVL  = 1.00f;    // @knob 0 1 0.01 group:Shape "Hand brightness"
const float HAND_TAIL = 3.00f;    // @knob 0 8 0.1 group:Shape "Hand trail (tubes)"
const float TAIL_LVL  = 0.30f;    // @knob 0 1 0.01 group:Shape "Trail brightness"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t   = millis() / 1000.0f;
  float clk = t * TIME_SCALE + TIME_OFFSET;

  float mins = fmod(clk / 60.0f, 60.0f);        // 0..60 within the hour
  float span = (float)(LINE1_LEN - SKIP_TOP - 1);
  float q    = span / 2.0f;                     // each line spans two quarter-hours

  int   half = mins < 30.0f ? 0 : 1;            // which line holds the hand
  float into = mins - (float)half * 30.0f;      // 0..30 minutes into that line
  float hPos = (float)(half * LINE1_LEN) + (into / 30.0f) * span;

  for (int i = 0; i < NUM_LEDS; i++) {
    int   line = i < LINE1_LEN ? 0 : 1;
    int   base = line * LINE1_LEN;
    float rel  = (float)(i - base);
    float v    = 0.0f;

    if (rel <= span) {
      // Quarter ticks: line ends and midpoints are :00 :15 :30 :45.
      for (int k = 0; k <= 2; k++) {
        float td = fabs(rel - (float)k * q);
        if (td >= 1.0f) continue;
        float tv = (1.0f - td) * TICK_LVL;
        if (tv > v) v = tv;
      }

      // The hand lives on one line at a time; the other shows only ticks.
      if (line == half) {
        float d = fabs((float)i - hPos);
        if (d < HAND_W) {
          float hv = (1.0f - d / HAND_W) * HAND_LVL;
          if (hv > v) v = hv;
        }
        float back = hPos - (float)i;
        if (back > 0.0f && back < HAND_TAIL) {
          float bv = (1.0f - back / HAND_TAIL) * TAIL_LVL;
          if (bv > v) v = bv;
        }
      }
    }

    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
