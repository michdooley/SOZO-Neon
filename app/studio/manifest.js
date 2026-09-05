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
];

export const PRETTY = (id) =>
  id.replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
