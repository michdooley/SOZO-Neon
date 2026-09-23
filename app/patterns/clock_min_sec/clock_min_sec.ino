// SOZO LED visualizer — clock: minutes on top, seconds below.
//
// The fast-moving sibling. Line 1 is the minute dial (0 at the bottom-left end
// to 60 at the far end) and line 2 is the second dial over the same span. The
// top 2 tubes of each line stay dark, leaving 12 tube-steps per dial: one step
// is 5 minutes up top and 5 seconds below, so the lower line visibly walks
// while the upper one creeps. Ticks mark the quarters on both. Read the minute
// hand for the time and let the second hand prove the piece is alive.

const int NUM_LEDS  = 30;
const int LINE1_LEN = 15;

const float TIME_SCALE  = 1.0f;      // @knob 1 600 1 group:Speed "Time scale (1 = real time)"
const float TIME_OFFSET = 2220.0f;   // @knob 0 3600 10 group:Position "Start time (s past the hour)"

const int   SKIP_TOP  = 2;        // @knob 0 4 1 group:Counts "Top tubes per line left dark"
const float TICK_LVL  = 0.10f;    // @knob 0 0.5 0.01 group:Shape "Quarter tick brightness"

const float MIN_W     = 1.00f;    // @knob 0.6 4 0.05 group:Shape "Minute hand width (tubes)"
const float MIN_LVL   = 0.85f;    // @knob 0 1 0.01 group:Shape "Minute hand brightness"
const float SEC_W     = 1.00f;    // @knob 0.6 4 0.05 group:Shape "Second hand width (tubes)"
const float SEC_LVL   = 1.00f;    // @knob 0 1 0.01 group:Shape "Second hand brightness"
const float SEC_TAIL  = 2.50f;    // @knob 0 6 0.1 group:Shape "Second hand trail (tubes)"
const float TAIL_LVL  = 0.28f;    // @knob 0 1 0.01 group:Shape "Trail brightness"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t   = millis() / 1000.0f;
  float clk = t * TIME_SCALE + TIME_OFFSET;

  float mins = fmod(clk / 60.0f, 60.0f);       // 0..60
  float secs = fmod(clk, 60.0f);               // 0..60

  float span = (float)(LINE1_LEN - SKIP_TOP - 1);
  float q    = span / 4.0f;
  float mPos = (mins / 60.0f) * span;                      // hand on line 1
  float sPos = (float)LINE1_LEN + (secs / 60.0f) * span;   // hand on line 2

  for (int i = 0; i < NUM_LEDS; i++) {
    int   base = i < LINE1_LEN ? 0 : LINE1_LEN;
    float rel  = (float)(i - base);
    float v    = 0.0f;

    if (rel <= span) {
      for (int k = 0; k <= 4; k++) {
        float td = fabs(rel - (float)k * q);
        if (td >= 1.0f) continue;
        float tv = (1.0f - td) * TICK_LVL;
        if (tv > v) v = tv;
      }

      float pos = base == 0 ? mPos : sPos;
      float wid = base == 0 ? MIN_W : SEC_W;
      float lvl = base == 0 ? MIN_LVL : SEC_LVL;

      float d = fabs((float)i - pos);
      if (d < wid) {
        float hv = (1.0f - d / wid) * lvl;
        if (hv > v) v = hv;
      }

      // Trail on the second hand only — it's the one you can see move.
      if (base == LINE1_LEN) {
        float back = pos - (float)i;
        if (back > 0.0f && back < SEC_TAIL) {
          float bv = (1.0f - back / SEC_TAIL) * TAIL_LVL;
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
