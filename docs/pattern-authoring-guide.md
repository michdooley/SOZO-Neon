# SOZO LED Pattern Guide

Reference for writing Arduino patterns for this LED art piece. Patterns are sent to the visualizer (or, eventually, the real fixture) over serial.

## Hardware summary

- **Two "lines"** of LEDs running roughly parallel across a 52″×38″ black backboard.
- **30 LEDs total**, each one a torus (donut) with 0.333″ cross-section.
- **Red only.** The single control per LED is **brightness, 0–255**.
- **15 LEDs per line.** Each line traces 3 humps; the two lines share one physical node (LED idx 26).
- Visualizer entry: [Visualizer/index.html](index.html). Run a local server (`python3 -m http.server 8000`) and connect serial from Chrome/Edge.

## Serial protocol

Per frame, send **30 integers 0–255**, comma- or space-separated, terminated with `\n`, at **115200 baud**.

```
255,200,180,...,0\n
```

The visualizer rejects any line whose int count isn't exactly 30 and shows what it received in the status bar.

Frame rate: ~30 fps (`delay(33)`) is comfortable. The full text frame is ≤120 bytes — fits easily in the baud budget.

## LED index map

Indices run **line 1 first (0–14), then line 2 (15–29)**, in path order along each line. (x, y) is in inches with the board centered at the origin, x right, y up. Outer radius `r` is in inches (each torus's visible outer ring; tube cross-section is 0.333″).

### Line 1 — idx 0–14 (15 LEDs)

| idx | bump | x (in) | y (in) | r (in) | notes |
|----:|:----:|------:|------:|------:|-------|
|  0 | 1 | −21.82 |  −9.52 | 2.19 | leftmost LED, largest torus |
|  1 | 1 | −19.58 |  −2.56 | 1.86 | |
|  2 | 1 | −16.45 |   3.06 | 1.72 | |
|  3 | 1 | −12.63 |   5.97 | 1.64 | **bump 1 peak** |
|  4 | 1 |  −8.33 |   4.54 | 1.48 | |
|  5 | 1 |  −5.46 |   1.21 | 1.48 | end of bump 1 |
|  6 | 2 |  −3.83 |   5.94 | 1.39 | |
|  7 | 2 |  −1.55 |  10.24 | 1.20 | |
|  8 | 2 |   1.35 |  12.83 | 1.11 | **bump 2 peak** |
|  9 | 2 |   4.93 |  12.26 | 1.02 | |
| 10 | 2 |   6.81 |   9.21 | 1.02 | end of bump 2 |
| 11 | 3 |   7.83 |  13.34 | 0.95 | |
| 12 | 3 |  10.10 |  16.27 | 0.95 | |
| 13 | 3 |  13.25 |  16.84 | 0.95 | **bump 3 peak** |
| 14 | 3 |  15.54 |  14.94 | 0.95 | end of line 1 |

### Line 2 — idx 15–29 (15 LEDs)

| idx | bump | x (in) | y (in) | r (in) | notes |
|----:|:----:|------:|------:|------:|-------|
| 15 | 1 |  −3.29 | −14.01 | 2.03 | lowest LED on board, largest torus |
| 16 | 1 |  −2.05 |  −7.86 | 1.87 | |
| 17 | 1 |  −0.19 |  −2.64 | 1.72 | |
| 18 | 1 |   2.37 |   0.63 | 1.64 | |
| 19 | 1 |   6.00 |   0.93 | 1.48 | **bump 1 peak / end** |
| 20 | 2 |   8.87 |  −1.96 | 1.48 | |
| 21 | 2 |   9.38 |   3.13 | 1.39 | |
| 22 | 2 |  10.58 |   7.04 | 1.20 | |
| 23 | 2 |  13.32 |   9.20 | 1.11 | **bump 2 peak** |
| 24 | 2 |  16.13 |   8.89 | 1.02 | end of bump 2 |
| 25 | 3 |  17.72 |   6.81 | 1.02 | |
| 26 | 3 |  18.20 |  10.66 | 0.95 | **shared node** (both lines cross here) |
| 27 | 3 |  19.33 |  13.49 | 0.95 | **bump 3 peak** |
| 28 | 3 |  21.75 |  14.37 | 0.95 | |
| 29 | 3 |  24.16 |  13.00 | 0.95 | rightmost LED |

### Index groupings at a glance

```
LINE 1 (top): 0  1  2  3  4  5 | 6  7  8  9 10 | 11 12 13 14
              ── bump 1 ──     ── bump 2 ──    ── bump 3 ──

LINE 2 (bot): 15 16 17 18 19 | 20 21 22 23 24 | 25 26 27 28 29
              ── bump 1 ──     ── bump 2 ──    ── bump 3 ──
                                                  ↑ shared
```

## Spatial structure & properties to exploit

- **Both lines move bottom-left → upper-right.** Walking the indices in order traverses each line spatially. A "chase" along `i = 0..29` produces a long sweep across the whole board.
- **Toruses get progressively smaller from idx 0 → idx 29** within each line. Big donuts cluster on the left; small ones cluster on the right. This is a useful texture for patterns that visually "fade" or "compress" toward one side.
- **Bumps are asymmetric peaks.** Each bump's "peak" (highest y) is the marked row above. Patterns can highlight bumps by lighting their peak strongest and softer toward the base.
- **Line 1 and Line 2 are not mirrors** — they have different total spans and different y-ranges. Don't assume `line1[i]` and `line2[i]` are at the same x; use the table above when spatial alignment matters.
- **The shared node (idx 26)** is the only LED where the two lines visually touch. Patterns can use it as a "crossing point" or sync trigger.

## Constraints to honor

- **Red only.** No color channel; only a brightness byte. Patterns that depend on color contrast won't translate — rely on brightness modulation, spatial position, and timing instead.
- **No HDR** — 255 is the max. Don't expect "extra bright" pixels to read differently from "full brightness".
- **30 fps is plenty.** Don't try to push thousands of frames per second; the eye and the LED hardware won't appreciate it.
- **Single physical strip per line is most likely.** Patterns that assume independent control of every LED are fine, but if hardware ends up grouping LEDs, that constraint will surface later.

## Pattern building blocks (Arduino)

All examples use this skeleton:

```cpp
const int NUM_LEDS = 30;
uint8_t buf[NUM_LEDS];

void setup() { Serial.begin(115200); }
void sendFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(buf[i]);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
}
void loop() {
  float t = millis() / 1000.0f;
  /* fill buf[] here */
  sendFrame();
  delay(33);
}
```

### Chase (comet sweeps the whole strip)

```cpp
float pos = fmod(t * 8.0f, NUM_LEDS);     // 8 LEDs/sec
for (int i = 0; i < NUM_LEDS; i++) {
  float d = fabs(i - pos);
  if (d > NUM_LEDS / 2.0f) d = NUM_LEDS - d;     // wrap
  float falloff = 1.0f - d / 5.0f;               // 5-LED tail
  buf[i] = falloff > 0 ? (uint8_t)(falloff * 255) : 0;
}
```

See [patterns/chase_test/chase_test.ino](patterns/chase_test/chase_test.ino) for the standalone version.

### Heartbeat (all in unison, smooth pulse)

```cpp
uint8_t b = (uint8_t)((sin(t * 1.6f) * 0.5f + 0.5f) * 255);
for (int i = 0; i < NUM_LEDS; i++) buf[i] = b;
```

### Bump-by-bump (each hump lights as a group, sequenced)

```cpp
// 6 bumps total (3 per line). Each lights for 0.7s.
int active = (int)(t / 0.7f) % 6;
int starts[] = { 0, 6, 11, 15, 20, 25 };
int ends[]   = { 5, 10, 14, 19, 24, 29 };
for (int i = 0; i < NUM_LEDS; i++) {
  buf[i] = (i >= starts[active] && i <= ends[active]) ? 255 : 0;
}
```

### Lines breathing in opposition

```cpp
float a = sin(t * 1.2f) * 0.5f + 0.5f;
float b = 1.0f - a;
for (int i = 0; i < 15;       i++) buf[i] = (uint8_t)(a * 255);   // line 1
for (int i = 15; i < NUM_LEDS; i++) buf[i] = (uint8_t)(b * 255);   // line 2
```

### Bump-peak emphasis (peaks brightest, fades to base of each hump)

```cpp
int peaks[]   = { 3,  8, 13, 19, 23, 27 };   // peak idx per bump (3 per line)
int radii[]   = { 3,  2,  1,  2,  2,  2 };   // how far the glow spreads
for (int i = 0; i < NUM_LEDS; i++) buf[i] = 0;
for (int p = 0; p < 6; p++) {
  for (int i = peaks[p] - radii[p]; i <= peaks[p] + radii[p]; i++) {
    if (i < 0 || i >= NUM_LEDS) continue;
    float d = fabs(i - peaks[p]);
    float v = 1.0f - d / (radii[p] + 1.0f);
    buf[i] = max(buf[i], (uint8_t)(v * 255));
  }
}
```

### Spatial wave by world x-position

If a pattern needs to feel like "left side lights first, sweeps right" regardless of the index ordering, use the actual x-coordinates from the index table. Hard-code them once:

```cpp
const float XS[NUM_LEDS] = {
  -21.82,-19.58,-16.45,-12.63,-8.33,-5.46,-3.83,-1.55,1.35,4.93,
   6.81,7.83,10.10,13.25,15.54,
  -3.29,-2.05,-0.19,2.37,6.00,8.87,9.38,10.58,13.32,16.13,
  17.72,18.20,19.33,21.75,24.16
};
// Sweep front from x=-22 to x=+25 over 4 seconds:
float sweepX = -22.0f + fmod(t / 4.0f, 1.0f) * 47.0f;
for (int i = 0; i < NUM_LEDS; i++) {
  float d = fabs(XS[i] - sweepX);
  buf[i] = d < 3.0f ? (uint8_t)((1.0f - d / 3.0f) * 255) : 0;
}
```

## Tips for designing new patterns

1. **Start with a story.** "A pulse climbs each hump in turn", "the two lines breathe opposite each other", "rain drops down the strip and pools at the bottom". Translate into brightness over time.
2. **Use the bump structure.** Patterns that respect the 3+3 hump grouping read as intentional; patterns that ignore it can feel arbitrary on this piece.
3. **Vary timing more than spatial complexity.** With only brightness to work with, slow builds and sudden drops are more legible than busy per-LED noise.
4. **Test in the visualizer first.** Idle flash = no serial. If your sketch is sending and you still see the flash, the serial protocol failed — check the status bar message before assuming the pattern is wrong.
5. **Mind the visual weight.** Idx 0 and idx 15 are huge donuts (2″+ radius); idx 11–14 and 25–29 are small. A pattern that lights "every LED at 128" will look much brighter on the left than the right — adjust per-LED if you want even visual weight.

## Quick reference

- 30 LEDs, indexed 0–29
- Bumps (start–end idx): L1B1 0–5, L1B2 6–10, L1B3 11–14, L2B1 15–19, L2B2 20–24, L2B3 25–29
- Bump peaks (brightest idx): 3, 8, 13, 19, 23, 27
- Shared node: idx 26
- Serial: 30 ints 0–255, comma/space-separated, `\n`, 115200 baud
