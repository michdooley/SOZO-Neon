// Ordering + fallback list of patterns the Studio can open.
//
// When you run `python3 studio/serve.py`, the Studio scans app/patterns/ live
// (so new patterns appear automatically); the ids listed here are shown first,
// in this order, and any extra patterns on disk are appended alphabetically.
// Over file:// or a plain `http.server` (no serve.py), this list is all the
// Studio has to go on.
//
// The 13 below are the SozoBasic firmware gallery, in gallery order — each is a
// selectable, tunable pattern whose default constants match what runs on the
// wall. Tune one and export it to refined-patterns/ (finalized).
export const PATTERNS = [
  'blob_gaussian',
  'pendulum',
  'tide',
  'sine_wave',
  'ramp_across',
  'chase_test_v2',
  'two_comets',
  'bump_cascade',
  'ripple',
  'recede_fill',
  'convergence',
  'bouncing_up',
  'random_flash',
  'bubbles_slow',
  'bubbles_fast_proper',
  'clock_hands',
  'clock_minute',
  'clock_min_sec',
  'crackling_fire',
];

// The SozoUnveiling bank, ported from the old show as individual patterns and
// rendered as its own folder/playlist in the Studio (see PLAYLISTS in main.js).
// Order = reconstructed unveiling-party play order (the Studio numbers them by
// this order): the vintage openers — basic flash/off, marquee dots, the flash+
// dot composite, the random morph, and the solid fade in/out — ran FIRST, then
// the "cool" gallery patterns launched. Files live in app/patterns/unv_<name>/.
export const UNVEILING = [
  // vintage openers
  'unv_solid',
  'unv_flash',
  'unv_dot_seq',
  'unv_dot_parallel',
  'unv_composite',
  'unv_random_morph',
  'unv_fade_in',
  'unv_fade_out',
  // then the "cool" gallery (firmware gallery-cycle order)
  'unv_blob_gaussian',
  'unv_pendulum',
  'unv_tide',
  'unv_sine_wave',
  'unv_ramp_across',
  'unv_chase',
  'unv_two_comets',
  'unv_stutter_chase',
  'unv_bump_cascade',
  'unv_ripple',
  'unv_recede_fill',
  'unv_convergence',
];

export const PRETTY = (id) =>
  id.replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
