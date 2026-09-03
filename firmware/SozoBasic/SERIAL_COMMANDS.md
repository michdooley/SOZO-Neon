# SozoBasic — Serial Commands

A small debug console built into [`SozoBasic.ino`](SozoBasic.ino) for testing the
show without waiting on the 4-hour clock or sitting through every stage.

## Connecting

- Open a Serial Monitor at **115200 baud**.
- Set the line ending to **Newline** (the sketch reads one command per line).
- Send `?` (or any unrecognized text) to print the command list.
- The console works at all times — during the show **and** after auto-shutoff.
  (Anything that changes state also clears the shutoff, so `reset`/`skip`/`next`
  revive the board if it has already turned off.)

## Timer commands

| Command | What it does |
|---|---|
| `skip <mins>` | Jump the shutoff clock so **`<mins>` minutes remain**. e.g. `skip 2`. `skip 0` shuts off immediately. The patterns keep running; only the timer moves. |
| `off` (or `shutoff`) | Shut off **now** (same as `skip 0`). The board goes dark and then does the once-a-minute reminder flash. |
| `flash` | Force shutoff and make the **reminder burst fire within ~1 s** (10 quick flashes), so you can eyeball it without waiting a minute. |
| `reset` | Restart the full **4-hour** timer and resume normal operation. |
| `status` (or `s`) | Print the timer state: minutes remaining + current stage, or, if shut off, how long ago. |

**Troubleshooting the end-of-timer behavior:** `skip 2` → watch it count down →
shut off → flash every minute. Or just `flash` to verify the flash instantly,
then `reset` when you're done.

## Stage / pattern commands

The show is a loop of 6 **stages**; the last stage (gallery) cycles a bank of
**gallery patterns**.

| Command | What it does |
|---|---|
| `stage` | Advance to the **next stage** (level test → vintage → random → solid-fade → fade-out → gallery → loop). |
| `next` (or `n`) | Advance to the **next gallery pattern** (jumps into the gallery first if you're not already there). Shown instantly at full brightness. |
| `pat <n>` | Jump straight to **gallery pattern number `<n>`** (see `list`). e.g. `pat 5`. |
| `list` (or `l`) | Print the gallery patterns with their index numbers. |

> Skipping to a gallery pattern shows it **instantly** (no 5-second fade) and
> holds it; after the normal 2-minute hold it resumes auto-cycling.

### The 6 stages

| # | Stage | |
|---|---|---|
| 0 | Level test | solid at 0/20/40/60/80/100%, 1 s each, ×2 |
| 1 | Vintage | the composite (flashes + dot sweeps) |
| 2 | Random morph | random flashes, rigid → fading |
| 3 | Solid fade-in | fade up to full solid (6 s) + hold |
| 4 | Fade-out | fade the whole board to black (2 s) |
| 5 | Gallery | cycle the gallery patterns, 2 min each |

### Gallery patterns (indices for `pat <n>`)

| n | Pattern | Source |
|---|---|---|
| 0 | `blob_gaussian` | default |
| 1 | `pendulum` | default |
| 2 | `tide` | refined |
| 3 | `sine_wave` | default |
| 4 | `ramp_across` | default |
| 5 | `chase_v2` | custom |
| 6 | `two_comets` | custom |
| 7 | `bump_cascade` | custom |
| 8 | `ripple` | default |
| 9 | `recede_fill` | default |
| 10 | `convergence` | default |
| 11 | `bouncing_up` | custom |
| 12 | `random_flash` | custom |

(`stutter_chase` was removed. Run `list` on the device to confirm the live order.)

## Notes

- These are **debug** commands; the installed show needs no serial connection —
  it just runs.
- The 4-hour auto-shutoff is driven by `millis()` (a hardware timer), so using
  these commands or the rendering load never affects its accuracy.
- After shutoff the board fires a burst of **10 quick flashes once per minute**
  as a reminder to cut the outlets; the onboard 12×8 LED matrix shows the
  minutes remaining while the show is running.
