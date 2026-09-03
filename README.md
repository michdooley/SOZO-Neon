# SOZO Neon

A neon-tube light sculpture: 30 dimmable neon tubes driven by an Arduino, plus a
browser-based **Pattern Studio** for designing, previewing, and tuning the light
patterns before they go on the hardware.

This repo has two halves:

- **`firmware/`** — the Arduino sketch that actually runs the show on the wall.
- **`app/`** — the Pattern Studio: a local web app where you write a pattern,
  watch it render in 3D, tune its parameters with sliders, and export it.

You author patterns in the Studio, then hand-integrate the good ones into the
firmware's gallery. This repo ships as a **clean slate** — no patterns of our own
in the working set, just a handful of reference sketches in `app/examples/` to
learn from.

---

## Hardware

- **Arduino Uno R4 WiFi** (uses the onboard 12×8 LED matrix for the shutoff
  countdown).
- **2× PCA9685** 16-channel PWM boards over I²C, at addresses `0x40` and `0x41`.
- **30 neon tubes**, each on its own PWM channel, driven **active-low** through
  their transformers (`INVERT_PWM = true` in the firmware).
- Physical layout: the 30 tubes sit at fixed X/Y positions on the board. Those
  world coordinates (in inches) live in `docs/coords.md` and `docs/mapping.md`,
  and are duplicated as the `XS[]` / `YS[]` arrays inside every pattern and the
  firmware. **If you rewire or reposition tubes, update the coordinates in all
  three places.**

Arduino libraries needed: **Adafruit PWM Servo Driver** (+ its BusIO
dependency), and **Arduino_LED_Matrix** (built in to the R4 core).

---

## Repo layout

```
SOZO-Neon/
├── firmware/
│   └── SozoBasic/
│       ├── SozoBasic.ino        # the runtime show (see "Firmware" below)
│       └── SERIAL_COMMANDS.md   # built-in serial debug console reference
├── app/                         # the Pattern Studio (web root)
│   ├── studio.html              # open this (via serve.py) to run the Studio
│   ├── editor/scene.js          # 3D preview (three.js, loaded from a CDN)
│   ├── studio/                  # Studio logic + local server
│   │   ├── main.js              # UI wiring
│   │   ├── transpile.js         # turns a pattern .ino into a live JS preview
│   │   ├── knobs.js             # builds the parameter sliders
│   │   ├── manifest.js          # pattern ordering + file:// fallback list
│   │   ├── directional.js       # per-pattern "play it reversed" specs
│   │   ├── reviews.js/.json     # star-ratings + notes you leave while tuning
│   │   └── serve.py             # local web server + save/duplicate/rename/delete
│   ├── patterns/                # ← selectable/tunable patterns (ships with the 13
│   │                            #    SozoBasic gallery patterns; add new ones here)
│   └── refined-patterns/        # ← finalized tuned exports land here
└── docs/
    ├── pattern-authoring-guide.md
    ├── pattern-generation.md
    ├── coords.md                # tube world-coordinates
    └── mapping.md               # LED index → PWM channel mapping
```

---

## Quickstart: run the Pattern Studio

You need **Python 3** (for the local server) and a modern browser. three.js is
pulled from a CDN, so you must be online the first time.

```bash
cd app
python3 studio/serve.py          # serves http://localhost:8000
```

Then open **http://localhost:8000/studio.html**.

> Why the server (and not just opening the file)? `serve.py` adds a `/save`
> endpoint so the Studio can write tuned sketches straight into
> `app/refined-patterns/` and manage patterns on disk. Plain
> `python3 -m http.server` also works, but then "Export" just downloads a file.

The Studio opens with the **13 SozoBasic gallery patterns** already listed —
select one to preview it in 3D and tune its sliders. Their default slider values
match exactly what runs on the wall. To create something new, add a pattern (next
section) and it appears immediately.

### The SozoBasic Gallery folder

In the left panel the 13 patterns are grouped and numbered (01–13, in firmware
gallery order) under a collapsible **SozoBasic Gallery** folder. Any new patterns
you add show up under a separate **Workspace** group.

The **▶** button on the folder header plays the **whole gallery as a combined
show**, exactly like the firmware's gallery stage: each pattern fades in, holds,
fades out, and the next begins — looping forever, with your saved tuning baked in.
The currently-playing pattern is highlighted. Two inputs under the header set the
timing: **sec / pattern** and **fade (s)**. They default to a watchable 8 s + 1.5 s;
set them to **120** and **5** to match the firmware exactly. Press **⏹** (or click
any pattern) to stop and return to tuning.

---

## Writing a pattern

A pattern is a single Arduino `.ino` sketch at
`app/patterns/<name>/<name>.ino`. The Studio treats the `.ino` as the **source of
truth**: it reads the file, transpiles `loop()` into a JavaScript preview, and
turns your top-of-file `const` values into sliders. There is no separate JS copy
to keep in sync.

The Studio understands a specific "house dialect" — follow this shape:

```cpp
const int NUM_LEDS = 30;

// Tube world-coordinates (inches). Copy verbatim from docs/coords.md.
const float XS[NUM_LEDS] = { /* 30 values */ };
// (add YS[] too if your pattern is 2D)

// Tunable constants become sliders in the Studio. Keep them simple numbers.
const float K     = 0.35f;   // radians per inch
const float OMEGA = 3.0f;    // radians per second

void setup() { Serial.begin(115200); }

void loop() {
  float t = millis() / 1000.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float v = sin(K * XS[i] + OMEGA * t) * 0.5f + 0.5f;   // 0.0..1.0
    int b = (int)(v * 255.0f);                            // 0..255
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\n');
  }
  delay(33);
}
```

Rules of the dialect (the transpiler targets exactly these):

- Top-of-file `const` scalars → sliders. `const` arrays (`XS`, `YS`, `PHASES`…)
  are read but not sliders.
- `loop()` iterates `NUM_LEDS`, computes a brightness per tube, and prints
  comma-separated `0..255` values ending in `\n`.
- Allowed math: `sin cos fmod exp sqrt fabs pow`, `PI`, `millis()`, `random()`.
- Stateful patterns (persistent globals / structs across frames) are supported.
- Anything outside the dialect still **exports fine** — you just lose the live
  preview (the Studio says "preview unavailable").

The fastest way to start: **duplicate one of the 13 shipped patterns** — use the
Studio's **⧉ Duplicate…** button, or on disk:

```bash
cp -R app/patterns/sine_wave app/patterns/my_pattern
mv app/patterns/my_pattern/sine_wave.ino app/patterns/my_pattern/my_pattern.ino
# edit it, then reload the Studio
```

See `docs/pattern-authoring-guide.md` and `docs/pattern-generation.md` for the
full guide.

---

## Workflow: idea → wall

1. **Author** — drop a `.ino` in `app/patterns/<name>/` (or duplicate one in the
   Studio UI).
2. **Preview & tune** — select it in the Studio, drag the sliders, watch the 3D
   render. Leave a star rating / notes if you want (saved to `reviews.json`).
3. **Export** — click **Export to refined-patterns/**. The tuned sketch (with
   your slider values baked into the `const`s) is written to
   `app/refined-patterns/<name>.ino`. Originals in `patterns/` are untouched.
4. **Integrate into the firmware** — the firmware's gallery is hand-written C, so
   copy your tuned pattern's `loop()` math into a new `patternXxx()` function in
   `firmware/SozoBasic/SozoBasic.ino` and add it to the `GAL_PATTERNS[]` table
   (see "Firmware" below).
5. **Flash** and enjoy.

---

## Firmware: `SozoBasic`

`SozoBasic.ino` is a low-maintenance, "it just works" looping show — no MIDI, no
remote, no input required. On power-up it runs a fixed loop of stages forever:

| Stage | What it does |
|-------|--------------|
| Level Test | Solid at 0/20/40/60/80/100 %, to check brightness |
| Vintage | A composite flash / dot-sequence show |
| Random Morph | Random flashes morphing from rigid to fading |
| Solid Fade | Fades up to full solid |
| Fade Out | Fades the whole board to black |
| Gallery | Cycles the pattern bank, ~2 min each (one full pass) |

Everything is drawn under a global tube-protection dim mask. A `millis()`-driven
**4-hour auto-shutoff** turns the neon off and then flashes the whole board 10×
once a minute (to cue cutting the outlets); the Uno R4's LED matrix shows the
minutes remaining, counting down.

**Adding a pattern to the gallery** — in `SozoBasic.ino`:

1. Write a `void patternMyThing(float t, float out[NUM_LEDS])` that fills `out[]`
   with `0.0..1.0` brightness values (port the math from your Studio pattern).
2. Add an entry to the `GAL_PATTERNS[]` table:
   `{ "my_thing", patternMyThing, NULL }` (the third field is an optional
   `init()` for stateful patterns — see `patternRandomFlash` / `initRandomFlash`).

The 13 patterns in `app/patterns/` are exactly the ones baked into this gallery,
in the same order, with their Studio slider defaults set to the tunings that run
here — so what you tune in the Studio matches the wall, and the Studio's ▶ Play
reproduces this gallery stage.

**Testing without waiting on the clock** — `SozoBasic` has a built-in serial
console (115200 baud): `skip <mins>`, `flash`, `reset`, `status`, `stage`,
`next`, `pat <n>`, `list`. Full reference in
`firmware/SozoBasic/SERIAL_COMMANDS.md`.

---

## Notes for whoever picks this up

- **`app/patterns/` is the Studio's working set** — it ships with the 13
  SozoBasic gallery patterns (selectable + tunable) and is where new patterns go.
  **`app/refined-patterns/` is finalized exports** (separate). Tuning a shipped
  pattern and exporting it writes to `refined-patterns/`, leaving the original in
  `patterns/` untouched.
- The Studio's 3D preview and the firmware share the same physical tube layout
  but express it differently (the preview uses SVG-derived positions in
  `scene.js`; patterns and firmware use the `XS/YS` inch coordinates). Keep them
  consistent with `docs/coords.md` if the hardware changes.
- Everything the Studio needs is vendored except **three.js**, which loads from
  `unpkg.com` (see the import map in `studio.html`). Swap that for a local copy
  if you need fully-offline use.
