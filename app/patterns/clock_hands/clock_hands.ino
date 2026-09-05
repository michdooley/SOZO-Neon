// SOZO LED visualizer — clock: hour + minute counts (bar readout).
//
// Two stacked bar gauges, one per line. The upper line (line 1, tubes 0–14)
// lights one tube per hour on a 12-hour clock; the lower line (line 2, tubes
// 15–29) lights one tube per completed 4 minutes (60 min / 15 tubes). So at
// 6:28 the upper line shows 6 lit tubes and the lower line shows 7 — a direct,
// countable readout rather than a swinging hand. Both bars fill from the start
// of their line. 12 o'clock lights all 12 hour tubes.
//
// End-of-hour transition: instead of the minute bar snapping from full to empty
// at :00, the last FADE_SECS of the hour drain it out from the bottom up, so it
// arrives at 0 minutes gracefully just as the hour tube ticks over.
//
// The clock is a pure function of t, so the controller can feed it wall-clock
// seconds directly. In the Studio, t starts at 0 — set TIME_OFFSET to the
// moment you want to inspect (defaults to 6:28 for a quick check) and raise
// TIME_SCALE to watch the bars advance.

const int NUM_LEDS  = 30;
const int LINE1_LEN = 15;

// --- time ---------------------------------------------------------------
const float TIME_SCALE  = 1.0f;       // @knob 1 3600 1  group:Speed    "Time scale (1 = real time)"
const float TIME_OFFSET = 23280.0f;   // @knob 0 43200 60 group:Position "Start time (s past 12:00)"

// --- brightness ---------------------------------------------------------
const float HOUR_LVL = 0.85f;         // @knob 0 1 0.01 group:Shape "Hour tube brightness"
const float MIN_LVL  = 1.00f;         // @knob 0 1 0.01 group:Shape "Minute tube brightness"

// --- end-of-hour minute drain -------------------------------------------
const float FADE_SECS = 8.0f;         // @knob 0 60 0.5  group:Transition "Minute drain length (s before :00)"
const float FADE_SOFT = 2.0f;         // @knob 0.5 6 0.1 group:Transition "Drain edge softness (tubes)"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t   = millis() / 1000.0f;
  float clk = t * TIME_SCALE + TIME_OFFSET;        // seconds of clock time

  float hrs  = fmod(clk / 3600.0f, 12.0f);         // 0..12
  float mins = fmod(clk / 60.0f, 60.0f);           // 0..60

  // Whole units elapsed. The small epsilon keeps exact times (e.g. 6:28:00)
  // from floating-point-rounding just under the intended count.
  float hourCount = floor(hrs + 0.0001f);          // 0..11 whole hours
  if (hourCount < 1.0f) hourCount = 12.0f;         // 12 o'clock lights all 12
  float minCount  = floor(mins / 4.0f + 0.0001f);  // 0..14, one tube per 4 min

  // Drain progress: 0 for most of the hour, ramping 0->1 over the final
  // FADE_SECS so the minute bar empties from the bottom before :00.
  float secOfHour = fmod(clk, 3600.0f);            // 0..3600
  float fadeP = 0.0f;
  if (FADE_SECS > 0.0f) {
    float startSec = 3600.0f - FADE_SECS;
    if (secOfHour >= startSec) fadeP = (secOfHour - startSec) / FADE_SECS;
  }
  // A soft wipe front that rises from below the bottom tube to above the top of
  // the bar as fadeP goes 0->1. Tubes below the front are dimmed, then dark.
  float front = fadeP * (minCount + FADE_SOFT) - FADE_SOFT;

  for (int i = 0; i < NUM_LEDS; i++) {
    float v = 0.0f;
    if (i < LINE1_LEN) {
      if ((float)i < hourCount) v = HOUR_LVL;      // fill tubes 0 .. hourCount-1
    } else {
      float local = (float)(i - LINE1_LEN);
      if (local < minCount) {
        float f = 1.0f;
        if (fadeP > 0.0f) {
          f = (local - front) / FADE_SOFT;         // 1 above the front, 0 below
          if (f > 1.0f) f = 1.0f;
          if (f < 0.0f) f = 0.0f;
        }
        v = MIN_LVL * f;
      }
    }
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
