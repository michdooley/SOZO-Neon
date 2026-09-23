# Archive — old / retro patterns

Reference-only material copied over from the original experimentation repo
(`0 - Experimentations`). Nothing here is live: it sits **outside** `app/patterns/`,
so the Studio does not scan or list it, and it is not part of the current firmware.
Kept for history and for pulling ideas back into new work.

## Contents

| Folder | What it is |
|--------|------------|
| `patterns/` | The full old Studio pattern bank (27), incl. ones dropped from the current gallery — `blob_sphere`, `bouncing_balls`, `divergence`, `echo_flash`, `lightning`, `lub_dub`, `pop_decay`, `rain`, `resonance`, `stutter_chase`, `twinkle`, plus `dumb_player` (a serial reader, not a generator) and a `js/` scratch folder. |
| `refined-patterns/` | Old tuned `.ino` exports (5). |
| `customized-patterns/` | Hand-customized sketches (6), including the original `chase_test(1).ino`. |
| `SozoUnveling/SozoUnveling.ino` | The old "unveiling" show firmware — its embedded 12-pattern gallery **and** the vintage/retro flash + dot-sequence scenes. |

## Notes

- These are the **pre-tuning** versions of patterns that also live in
  `app/patterns/`. The active copies there have their defaults set to the tunings
  that actually run on the wall; treat these archived ones as the older baseline.
- To bring one back to life, copy its folder into `app/patterns/<id>/` (a
  `<id>/<id>.ino` layout) and reload the Studio.
