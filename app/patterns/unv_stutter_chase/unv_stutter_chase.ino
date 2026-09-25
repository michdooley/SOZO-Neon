// SOZO — SozoUnveiling bank: stutter chase.
//
// A single comet that randomly stutters — mostly forward, sometimes it holds,
// sometimes it darts backward — for a glitchy, restless motion. Stateful: the
// position and current mode carry across frames.
const int   NUM_LEDS  = 30;
const float SPEED_FWD = 9.0f;    // @knob 0 25 0.5   group:Speed "Forward speed (tubes/s)"
const float SPEED_REV = -7.0f;   // @knob -25 0 0.5  group:Speed "Reverse speed (tubes/s)"
const float TAIL      = 5.0f;    // @knob 1 12 0.5   group:Shape "Tail length (tubes)"

// state carried across frames (one declaration per line so the Studio picks
// each up as persistent state)
float scPos = 0.0f;
float scLastT = 0.0f;
float scModeUntil = 0.0f;
int   scMode = 0;

void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f;
  float sdt = t - scLastT;
  if (sdt > 0.1f) sdt = 0.033f;
  if (sdt < 0.0f) sdt = 0.0f;
  scLastT = t;

  if (t >= scModeUntil) {
    int r = random(100);
    scMode = (r < 65) ? 0 : (r < 88 ? 1 : 2);
    scModeUntil = t + 0.12f + (random(1000) / 1000.0f) * 0.55f;
  }
  float speed = (scMode == 0) ? SPEED_FWD : (scMode == 1 ? 0.0f : SPEED_REV);
  scPos += speed * sdt;
  while (scPos < 0.0f) scPos += NUM_LEDS;
  while (scPos >= NUM_LEDS) scPos -= NUM_LEDS;

  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - scPos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / TAIL;
    float v = f > 0.0f ? f : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
