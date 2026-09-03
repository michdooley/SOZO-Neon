// Reversal specs for the directional patterns.
//
// Two kinds of reverse, both of which bake cleanly into the exported .ino:
//   - negate: flip the sign of a direction constant (exported as a negated
//     constant value — pure parameterization, no logic change).
//   - source: a minimal structural rewrite of loop() (exported verbatim).
//
// `axis` is used only by the headless verifier (jsc_reverse.js) to confirm
// the propagation direction actually flips. Originals in patterns/ are never
// touched; these transforms apply to a copy for preview + export.

export const DIRECTIONAL = {
  // --- sign-flip a velocity/rate constant ---
  sine_wave: { axis: 'x', negate: ['OMEGA'] },
  tide: { axis: 'x', negate: ['TIDE_HZ'] },
  stutter_chase: { axis: 'index', negate: ['SPEED_FWD', 'SPEED_REV'] },

  // --- structural one-liners ---
  chase_test: {
    axis: 'index',
    source: (s) =>
      s.replace(
        'float pos = fmod(t * CHASE_SPEED, (float)NUM_LEDS);',
        'float pos = ((float)NUM_LEDS - fmod(t * CHASE_SPEED, (float)NUM_LEDS));',
      ),
  },
  two_comets: {
    axis: 'index',
    source: (s) =>
      s
        .replace(
          'float posA = fmod(t * SPEED_A, (float)LINE1_LEN);',
          'float posA = ((float)LINE1_LEN - fmod(t * SPEED_A, (float)LINE1_LEN));',
        )
        .replace(
          'float posB = fmod(t * SPEED_B, (float)LINE2_LEN);',
          'float posB = ((float)LINE2_LEN - fmod(t * SPEED_B, (float)LINE2_LEN));',
        ),
  },
  bump_cascade: {
    axis: 'index',
    source: (s) =>
      s.replace(
        'float localT = cycleT - p * STAGGER;',
        'float localT = cycleT - (N_BUMPS - 1 - p) * STAGGER;',
      ),
  },
  ramp_across: {
    axis: 'x',
    source: (s) =>
      s
        .replace(
          'float u = fmod(t / SWEEP_PERIOD, 1.0f);',
          'float u = 1.0f - fmod(t / SWEEP_PERIOD, 1.0f);',
        )
        .replace('float d = leadX - XS[i];', 'float d = XS[i] - leadX;'),
  },
  blob_sphere: {
    axis: 'x',
    source: (s) =>
      s.replace(
        'float cx = X_MIN + u * (X_MAX - X_MIN);',
        'float cx = X_MAX - u * (X_MAX - X_MIN);',
      ),
  },
  divergence: {
    axis: 'radius',
    source: (s) =>
      s
        .replace('float r = u * MAX_R;', 'float r = (1.0f - u) * MAX_R;')
        .replace('if (d <= r && d >= r - TAIL) {', 'if (d >= r && d <= r + TAIL) {')
        .replace('v = 1.0f - (r - d) / TAIL;', 'v = 1.0f - (d - r) / TAIL;'),
  },
  convergence: {
    axis: 'radius',
    source: (s) =>
      s.replace(
        'float r = MAX_R * (1.0f - u / CYCLE);',
        'float r = MAX_R * (u / CYCLE);',
      ),
  },
};

// Apply reversal to a (param-baked) source + params pair.
export function applyReverse(spec, source, params) {
  if (!spec) return { source, params };
  let src = source;
  let p = params;
  if (spec.source) src = spec.source(src);
  if (spec.negate) {
    p = { ...params };
    for (const name of spec.negate) if (name in p) p[name] = -p[name];
  }
  return { source: src, params: p };
}
