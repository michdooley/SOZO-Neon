// SOZO LED dumb player.
//
// Receives 30 brightness ints per frame from the host (the browser-based
// pattern editor) and writes them to the LEDs. The host is the source of
// truth for animation; this sketch just parses and forwards.
//
// Protocol matches the rest of the project:
//   v,v,v,...,v\n   — 30 ints 0..255, comma- or whitespace-separated
//                     115200 baud
//
// Drop your LED-driver call inside writeLeds() — the brightness[] array is
// pre-populated by the time it's called. If you're using FastLED, set each
// strip pixel's red channel to brightness[i] there.

const int NUM_LEDS = 30;
uint8_t brightness[NUM_LEDS];

void writeLeds() {
  // TODO: push brightness[] to your LED hardware here.
  //
  // Example (FastLED, single red channel):
  //   for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(brightness[i], 0, 0);
  //   FastLED.show();
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < NUM_LEDS; i++) brightness[i] = 0;
}

static char lineBuf[256];
static int  lineLen = 0;

void handleLine() {
  if (lineLen == 0) return;
  lineBuf[lineLen] = '\0';

  int n = 0;
  char* p = lineBuf;
  while (n < NUM_LEDS && *p) {
    while (*p && (*p == ',' || *p == ' ' || *p == '\t')) p++;
    if (!*p) break;
    int v = atoi(p);
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    brightness[n++] = (uint8_t)v;
    while (*p && *p != ',' && *p != ' ' && *p != '\t') p++;
  }
  if (n == NUM_LEDS) writeLeds();
  // If n != NUM_LEDS the frame was malformed; silently drop it.
}

void loop() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\n' || c == '\r') {
      handleLine();
      lineLen = 0;
    } else if (lineLen < (int)sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = (char)c;
    } else {
      lineLen = 0;   // overflow — drop and resync on next newline
    }
  }
}
