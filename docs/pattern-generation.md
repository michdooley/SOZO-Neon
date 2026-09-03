# SOZO Pattern Generation & Refinement Guide

**You are a Claude instance helping refine and create LED patterns for the SOZO
art piece.** A human reviews patterns in a browser "Studio," rates/flags them,
and leaves notes on what to change. Your job is to read those notes and the
pattern source, then **edit or create `.ino` pattern files** so they look and
feel the way the human wants — while staying inside the constraints below.

This guide is self-contained. Read it fully before your first edit. The deeper
geometry reference is [`pattern_guide.md`](pattern_guide.md); the exact preview
rules are enforced by [`studio/transpile.js`](studio/transpile.js).

---

## 1. The physical piece

- A **52″ × 38″ black backboard** with **two "lines" of LEDs** running roughly
  parallel, bottom-left → upper-right.
- **30 LEDs total**, each a **torus (donut)**. **Red only** — the *only* control
  per LED is **brightness, 0–255**. No color, no HDR (255 is just "full").
- **15 LEDs per line.** Each line traces **3 humps ("bumps")**. The two lines
  share **one physical node at LED idx 26**.
- Toruses shrink from idx 0 → 29 within each line: **big donuts on the left,
  small on the right.** A flat "128 everywhere" reads brighter on the left.

You are designing **brightness over space and time.** With no color, your tools
are: spatial position, timing (builds, holds, drops), and per-LED modulation.

---

## 2. LED map & coordinates

Indices run **line 1 first (0–14), then line 2 (15–29)**, in path order along
each line. `(x, y)` is in inches, board centered at origin, x→right, y→up.

```
LINE 1 (top): 0  1  2  3  4  5 | 6  7  8  9 10 | 11 12 13 14
              ── bump 1 ──       ── bump 2 ──    ── bump 3 ──
LINE 2 (bot): 15 16 17 18 19 | 20 21 22 23 24 | 25 26 27 28 29
              ── bump 1 ──       ── bump 2 ──    ── bump 3 ──
                                                    ↑ idx 26 = shared node
```

- **Bump ranges:** L1: 0–5, 6–10, 11–14 · L2: 15–19, 20–24, 25–29
- **Bump peaks (brightest idx):** 3, 8, 13, 19, 23, 27
- **Shared node:** idx 26

Paste these arrays verbatim into any spatial pattern (units = inches):

```cpp
const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};
const float YS[NUM_LEDS] = {
   -9.52f, -2.56f,  3.06f,  5.97f,  4.54f,  1.21f,  5.94f, 10.24f, 12.83f, 12.26f,
    9.21f, 13.34f, 16.27f, 16.84f, 14.94f,
  -14.01f, -7.86f, -2.64f,  0.63f,  0.93f, -1.96f,  3.13f,  7.04f,  9.20f,  8.89f,
    6.81f, 10.66f, 13.49f, 14.37f, 13.00f
};
```

---

## 3. Where patterns live (the pipeline)

```
patterns/<id>/<id>.ino   →   Studio (tune + review)   →   refined-patterns/<id>.ino   →   SozoCumulative.ino
  (logic source              human previews,                exported tuned copy            hand-ported into the
   of truth)                 rates, flags, notes            (blessed version)              shipping controller)
```

- **The Studio loads `patterns/<id>/<id>.ino`.** With the local server running it
  **auto-discovers** every `patterns/<id>/` folder (so a new file just appears in
  the list); `studio/manifest.js` is the fallback order when the server is off.
  To change a pattern's **behavior/logic**, edit that file — that's what shows in
  the preview.
- The human can also **Duplicate / Rename / Delete** patterns from the Studio
  (the duplicate bakes in the current slider values — that's how they spin up
  tuning variants like `tide_fast`). Those land as ordinary `patterns/<id>/`
  folders, same as anything you create.
- **Export** writes the tuned result to **`refined-patterns/<id>.ino`** (only
  the scalar `const` values change; logic is copied byte-for-byte). These are
  the human-approved versions.
- The shipping controller [`../SozoCumulative/SozoCumulative.ino`](../SozoCumulative/SozoCumulative.ino)
  contains hand-ported copies of the refined patterns (see §8).

**Rule of thumb:** edit `patterns/<id>/<id>.ino` for logic; the human handles
export. Don't edit `refined-patterns/` by hand unless asked — it's an output.

---

## 4. Anatomy of a pattern `.ino`

Every pattern is a standalone Arduino sketch that prints **30 brightness ints
(0–255), comma-separated, newline-terminated, at 115200 baud**, ~30 fps.

```cpp
// SOZO LED visualizer — <name> pattern.
//
// <One-sentence story of what it does. The 2nd comment line is shown as the
//  pattern's description in the Studio, so make it a good gloss.>

const int NUM_LEDS = 30;

// ... const scalars (these become Studio sliders) ...
// ... const arrays (XS/YS/PHASES/...) if spatial ...

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;       // seconds since boot

  for (int i = 0; i < NUM_LEDS; i++) {
    float v = /* brightness in [0,1] for LED i at time t */;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);                          // ~30 fps
}
```

For patterns that build a buffer first (e.g. additive/overlapping effects):

```cpp
uint8_t buf[NUM_LEDS];
// ... fill buf[i] ...
for (int i = 0; i < NUM_LEDS; i++) {
  Serial.print(buf[i]); Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
}
```

---

## 5. The transpiler contract (READ THIS — it governs preview)

The Studio previews a pattern by **transpiling only its `loop()` body** to
JavaScript. If your pattern uses something outside the supported vocabulary, the
preview falls back to "unavailable" (edit + export still work, but the human
can't *see* it). **Keep every pattern previewable.**

**✅ Supported inside `loop()`:**
- Math (mapped to JS): `fmod sin cos tan exp sqrt pow log atan2 fabs abs floor
  ceil min max` and the constant `PI`.
- `millis()` → the Studio clock (so `millis()/1000.0f` is your seconds).
- `random(hi)` / `random(lo, hi)` → a **deterministic** PRNG. *Preview is the
  same every run* — good for reproducibility; don't expect true randomness in
  preview.
- `for` loops, `if/else`, ternaries, local `float/int` vars, local
  `uint8_t buf[NUM_LEDS];`, and C casts like `(int)`, `(float)`, `(uint8_t)`.
- Top-of-file `const int` / `const float` scalars and `const float NAME[...] = {…}`
  arrays.
- Stateful patterns: a `struct {…};` plus mutable globals declared before
  `setup()` (scalars, or arrays of structs). State persists across frames and is
  reset when the pattern (re)loads.

**❌ Not supported (breaks preview):**
- **Custom helper functions.** *Only `loop()` is transpiled.* If `loop()` calls
  a function you defined, the preview errors. **Inline all logic into `loop()`.**
- Pointers, classes/methods, `String`, `Serial.read()`/input parsing, `#define`
  macros for logic, multi-dimensional arrays, switch statements.
- Anything in `setup()` beyond `Serial.begin` (setup is not transpiled).

If the Studio status bar says *"preview unavailable (…)"*, you used something
off this list. Rework it into the supported subset.

---

## 6. `@knob` annotations (tuning metadata)

Any editable scalar `const` becomes a Studio slider. Give it a sane range, group,
and label with a trailing `// @knob` comment **on the same line** — this keeps
tuning metadata in the source of truth:

```cpp
const float CHASE_SPEED = 8.0f;   // @knob 0.5 25 0.1 group:Speed "Speed (LEDs/s)"
const float TAIL        = 5.0f;   // @knob 1 12 0.5 group:Shape "Tail length"
const int   N_RINGS     = 4;      // @knob 1 9 1 group:Counts "Ring count"
```

Format: `@knob <min> <max> <step> group:<Group> "<Label>"` — all parts optional;
the three numbers are read in order. **Groups** (display order): `Speed`,
`Overlap`, `Shape`, `Position`, `Counts`, `Other`. Without an annotation, the
Studio guesses a range from the name/magnitude — usable, but annotate anything
you want tuned precisely. Names matching `NUM_LEDS`, `LINE1_LEN`, `LINE2_LEN` are
hidden (structural).

> When you change ranges, remember a count/size knob (names starting `N_` or
> `MAX_`) resets pattern state on change so arrays resize.

---

## 7. The review loop (how the human talks to you)

In the Studio's left panel the human filters by **flag** and **rating**; on each
pattern they set:
- **Rating** ★0–5
- **Flag** — `✓` accept · `✕` reject · `—` none
- **Notes** — free text: *what to improve / change.*

This is persisted to **[`studio/reviews.json`](studio/reviews.json)** (when the
local server is running). If that file exists, **read it** — it's the human's
direct feedback, keyed by pattern id:

```json
{
  "two_comets": { "rating": 4, "flag": "accept", "notes": "love the drift; make the tails a touch longer" },
  "tide":       { "rating": 2, "flag": "reject", "notes": "too slow and even — needs a clearer high/low side" }
}
```

Treat a `reject` + notes as a concrete change request. Treat `accept` as "don't
regress this." When you finish an edit, summarize what you changed and why, in
terms of the note you addressed.

---

## 8. Downstream: SozoCumulative & the brightness ramp

The shipping controller hand-ports each refined pattern into:

```cpp
void patternX(float t, float out[NUM_LEDS]);   // fills out[i] in [0,1]
```

So write patterns that port cleanly:
- **Prefer pure functions of `t`.** A pattern that's fully determined by `t`
  (no carried state) ports trivially and behaves identically forward/reverse.
- **If stateful, keep state in a few clearly-named globals** and make sure it
  reinitializes from zero each time the pattern starts (the controller calls an
  `init()` on entry). Avoid relying on long warm-up history.
- The controller renders at higher FPS than 30 — patterns are continuous
  functions of `t`, so that's fine; never assume a fixed frame count.

**The global brightness ramp.** On the real fixture a per-LED mask dims each
line from 100% down to a **60% floor** by ~halfway through the middle bump, then
holds 60% to the end. Practically: **the right third of each line (≈ idx 9–14
and 23–29) renders dimmer on hardware.** Don't hinge a pattern's payoff on subtle
brightness differences out there. The Studio can preview this ramp (the "Global
Brightness Ramp" panel — toggle it on to see the real look).

---

## 9. Design principles

1. **Start with a story.** "A pulse climbs each hump in turn," "the two lines
   breathe in opposition," "rings drop from the shared node." Translate the
   story into brightness-over-time.
2. **Respect the bump structure.** Patterns that honor the 3+3 hump grouping or
   the peaks (idx 3,8,13,19,23,27) read as intentional. Ignoring it can feel
   arbitrary.
3. **Vary timing more than spatial complexity.** With brightness-only, slow
   builds and sudden drops are far more legible than busy per-LED noise.
4. **Use world coordinates for spatial moves.** For "sweeps left→right" or
   "radiates from the node," use `XS/YS`, not index order — index order isn't
   linear in space.
5. **Mind visual weight.** Big left donuts (idx 0, 15) dominate; small right
   ones recede. Compensate per-LED if you want even weight.
6. **The shared node (idx 26)** is the one place the lines touch — a natural
   origin, sync point, or collision target.

---

## 10. Creating a NEW pattern — checklist

1. Pick a short snake_case `id` (e.g. `cross_fade`).
2. Create `patterns/<id>/<id>.ino` using the §4 skeleton. **Make the 2nd comment
   line a good one-line description** (the Studio shows it).
3. Keep all logic in `loop()`; use only the §5 vocabulary.
4. Add `@knob` annotations (§6) to every tunable const.
5. With the server running it appears automatically. For it to show when the
   server is *off*, also add `'<id>'` to the `PATTERNS` array in
   [`studio/manifest.js`](studio/manifest.js).
6. Tell the human to open the Studio and preview it; confirm the status bar
   doesn't say "preview unavailable."

---

## 11. REFINING an existing pattern — checklist

1. Read the note (from `reviews.json` or the human) and the current
   `patterns/<id>/<id>.ino`.
2. Make the smallest change that addresses the note. Preserve what's working
   (especially on `accept`-flagged patterns).
3. Stay previewable (§5) and keep/adjust `@knob` ranges so the human can fine-tune.
4. If you add a tunable behavior, expose it as a new `const` + `@knob` rather
   than hard-coding a magic number.
5. Summarize: "Addressed *<note>* by *<change>*; new knobs: *<…>*."

---

## 12. Worked example (full, previewable template)

A spatial sweep that radiates outward from the shared node — copy this shape:

```cpp
// SOZO LED visualizer — node pulse pattern.
//
// Soft rings of light are born at the shared node (idx 26) and expand outward
// across both lines, fading as they grow — like a slow heartbeat from the seam.

const int NUM_LEDS = 30;

const float XS[NUM_LEDS] = {
  -21.82f,-19.58f,-16.45f,-12.63f, -8.33f, -5.46f, -3.83f, -1.55f,  1.35f,  4.93f,
    6.81f,  7.83f, 10.10f, 13.25f, 15.54f,
   -3.29f, -2.05f, -0.19f,  2.37f,  6.00f,  8.87f,  9.38f, 10.58f, 13.32f, 16.13f,
   17.72f, 18.20f, 19.33f, 21.75f, 24.16f
};
const float YS[NUM_LEDS] = {
   -9.52f, -2.56f,  3.06f,  5.97f,  4.54f,  1.21f,  5.94f, 10.24f, 12.83f, 12.26f,
    9.21f, 13.34f, 16.27f, 16.84f, 14.94f,
  -14.01f, -7.86f, -2.64f,  0.63f,  0.93f, -1.96f,  3.13f,  7.04f,  9.20f,  8.89f,
    6.81f, 10.66f, 13.49f, 14.37f, 13.00f
};

const float OX        = 18.20f;   // shared node x (idx 26)
const float OY        = 10.66f;   // shared node y
const float PULSE_HZ  = 0.30f;    // @knob 0.05 1.5 0.01 group:Speed  "Pulse rate (Hz)"
const float RING_VEL  = 16.0f;    // @knob 4 40 0.5    group:Speed  "Expansion speed (in/s)"
const float RING_W    = 5.0f;     // @knob 1 14 0.5    group:Shape  "Ring thickness (in)"
const float MAX_R     = 46.0f;    // @knob 20 70 1     group:Shape  "Fade-out radius (in)"

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float period = 1.0f / PULSE_HZ;
  float age    = fmod(t, period);          // seconds since the live ring was born
  float r      = age * RING_VEL;            // its current radius

  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - OX, dy = YS[i] - OY;
    float d  = sqrt(dx * dx + dy * dy);
    float diff = fabs(d - r);
    float v = 0.0f;
    if (diff < RING_W) v = (1.0f - diff / RING_W) * (1.0f - r / MAX_R);
    if (v < 0.0f) v = 0.0f;

    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
```

Why it previews cleanly: all logic is in `loop()`, it uses only
`fmod/sqrt/fabs`, the consts are annotated, and the output is the standard
30-int tail. Browse [`refined-patterns/`](refined-patterns/) for a dozen more
real examples (chase, ripple, tide, pendulum, bump_cascade, convergence, …).

---

## 13. Quick reference

| | |
|---|---|
| LEDs | 30, idx 0–29 (line 1 = 0–14, line 2 = 15–29) |
| Bumps | L1: 0–5 / 6–10 / 11–14 · L2: 15–19 / 20–24 / 25–29 |
| Peaks | 3, 8, 13, 19, 23, 27 · Shared node: 26 |
| Output | 30 ints 0–255, comma/space-sep, `\n`, 115200 baud, ~30 fps |
| Color | red only — brightness 0–255, no HDR |
| Preview math | `fmod sin cos tan exp sqrt pow log atan2 fabs abs floor ceil min max PI` |
| Preview time/rng | `millis()` = clock · `random()` = deterministic |
| Hard rule | all logic inside `loop()`; **no helper functions** |
| Far-end caveat | idx ~9–14 & ~23–29 render at ~60% on hardware |
| Logic source | `patterns/<id>/<id>.ino` (also register in `studio/manifest.js`) |
| Tuned output | `refined-patterns/<id>.ino` (Studio export) |
