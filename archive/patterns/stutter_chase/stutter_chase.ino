// SOZO LED visualizer — stutter chase pattern.
//
// A chase that occasionally hiccups: pauses for a beat, jitters back,
// or briefly reverses before continuing forward. Mostly forward motion,
// but the random stalls and stutters make it feel like a chase that's
// just slightly broken. Position state is preserved across loop() calls.

const int   NUM_LEDS  = 30;
const float SPEED_FWD = 9.0f;
const float SPEED_REV = -7.0f;
const float TAIL      = 5.0f;

float pos        = 0.0f;
float lastT      = 0.0f;
float modeUntil  = 0.0f;
int   mode       = 0;   // 0 = forward, 1 = paused, 2 = reverse

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
}

void loop() {
  float t  = millis() / 1000.0f;
  float dt = t - lastT;
  if (dt > 0.1f) dt = 0.033f;   // first frame / overruns
  lastT = t;

  if (t >= modeUntil) {
    int r = random(100);
    if      (r < 65) mode = 0;
    else if (r < 88) mode = 1;
    else             mode = 2;
    float dur = 0.12f + (random(1000) / 1000.0f) * 0.55f;
    modeUntil = t + dur;
  }

  float speed = (mode == 0) ? SPEED_FWD : (mode == 1 ? 0.0f : SPEED_REV);
  pos += speed * dt;
  while (pos < 0.0f)      pos += NUM_LEDS;
  while (pos >= NUM_LEDS) pos -= NUM_LEDS;

  for (int i = 0; i < NUM_LEDS; i++) {
    float d = fabs((float)i - pos);
    if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;
    float f = 1.0f - d / TAIL;
    int b = f > 0.0f ? (int)(f * 255.0f) : 0;
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
