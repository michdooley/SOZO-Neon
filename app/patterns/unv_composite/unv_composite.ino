// SOZO — SozoUnveiling vintage: composite medley (the signature opener).
// A fixed sequence, looping: 3x FLASH, 1x DOT-SEQ, 3x FLASH, 2x DOT-PARALLEL.
// Raise SPEED to preview the whole ~64 s cycle faster.
const int NUM_LEDS = 30;
const int ORDER[NUM_LEDS] = {
  15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
};
const float ON_SEC   = 2.0f;   // @knob 0.05 6 0.05 group:Flash "Flash on (s)"
const float OFF_SEC  = 0.6f;   // @knob 0.05 6 0.05 group:Flash "Flash off (s)"
const float STEP_SEC = 0.8f;   // @knob 0.05 3 0.05 group:Dots  "Dot step (s)"
const float LEVEL    = 1.0f;   // @knob 0 1 0.01    group:Shape "Brightness"
const float SPEED    = 1.0f;   // @knob 1 20 0.5    group:Speed "Preview speed-up"
void setup() { Serial.begin(115200); }
void loop() {
  float t = millis() / 1000.0f * SPEED;
  float flashLoop = ON_SEC + OFF_SEC;
  float seqLoop   = NUM_LEDS * STEP_SEC;
  float parLoop   = 15.0f * STEP_SEC;
  float seg1 = 3.0f * flashLoop;   // 3x flash
  float seg2 = seqLoop;            // 1x dot-seq
  float seg3 = 3.0f * flashLoop;   // 3x flash
  float seg4 = 2.0f * parLoop;     // 2x dot-parallel
  float total = seg1 + seg2 + seg3 + seg4;
  float el = fmod(t, total);

  int   mode = 0;      // 0 flash, 1 dot-seq, 2 dot-parallel
  float lel  = 0.0f;   // time since this segment started
  if (el < seg1) { mode = 0; lel = el; }
  else if (el < seg1 + seg2) { mode = 1; lel = el - seg1; }
  else if (el < seg1 + seg2 + seg3) { mode = 0; lel = el - (seg1 + seg2); }
  else { mode = 2; lel = el - (seg1 + seg2 + seg3); }

  bool flashOn = fmod(lel, flashLoop) < ON_SEC;
  int  seqStep = floor(lel / STEP_SEC); seqStep = seqStep % NUM_LEDS;
  int  litSeq  = ORDER[seqStep];
  int  parStep = floor(lel / STEP_SEC); parStep = parStep % 15;

  for (int i = 0; i < NUM_LEDS; i++) {
    float v = 0.0f;
    if (mode == 0) v = flashOn ? LEVEL : 0.0f;
    else if (mode == 1) v = (i == litSeq) ? LEVEL : 0.0f;
    else v = (i == parStep || i == 15 + parStep) ? LEVEL : 0.0f;
    int b = (int)(v * 255.0f);
    Serial.print(b); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
