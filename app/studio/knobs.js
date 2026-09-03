// Turn parsed .ino consts into grouped, range-bounded editor knobs.
//
// Range/label/group can be set explicitly in the .ino with a trailing
// annotation on the const's line — this keeps tuning metadata in the
// source of truth:
//
//   const float CHASE_SPEED = 8.0f;   // @knob 0.5 25 0.1 group:Speed "Speed (LEDs/s)"
//
// Without an annotation we fall back to name-based heuristics.

// Consts that are structural/hardware, never shown as sliders.
const HIDDEN = new Set(['NUM_LEDS', 'LINE1_LEN', 'LINE2_LEN']);

const GROUP_ORDER = ['Speed', 'Overlap', 'Shape', 'Position', 'Counts', 'Other'];

function classify(name) {
  const n = name.toUpperCase();
  if (/(SPEED|_HZ|HZ$|OMEGA|BPM|RATE|VEL|PERIOD|TRAVEL|SWEEP|RISE|DECAY)/.test(n)) return 'Speed';
  if (/(OVERLAP|DELAY|STAGGER|REST|GAP|SPACING|PHASE)/.test(n)) return 'Overlap';
  if (/(SIGMA|RADIUS|_R$|^R_|WIDTH|TAIL|SPREAD|EDGE|AMP|SWING|BASE|FLOOR|SCALE|DIM|FACTOR|_W$|MAX_R|SPHERE|RING_W|HALF|_K$|^K$)/.test(n)) return 'Shape';
  if (/(^CX|^CY|^OX|^OY|X_MIN|X_MAX|Y_MIN|Y_MAX|X_LO|X_HI|Y_TOP|Y_BOT|Y_BOB|^X_|^Y_|VY0)/.test(n)) return 'Position';
  if (/(^N_|^MAX_|_LEN$|^NUM_|COUNT|PULSES|RINGS|DROPS|POPS|FLASHES|ECHOES|STRIKES)/.test(n)) return 'Counts';
  return 'Other';
}

function isBidirectional(name, def) {
  if (def < 0) return true;
  return /(OMEGA|TILT|VY0|SPEED_REV|^CX$|^CY$|X_MIN|Y_MIN|_LO$|VEL_REV)/.test(name.toUpperCase());
}

function heuristicRange(name, def, type) {
  const bidir = isBidirectional(name, def);
  if (type === 'int') {
    const mag = Math.max(1, Math.abs(def));
    return {
      min: bidir ? -mag * 2 : 0,
      max: Math.max(2, Math.round(mag * 3)),
      step: 1,
    };
  }
  const mag = Math.abs(def) || 1;
  if (bidir) return { min: -mag * 2, max: mag * 2, step: (mag * 4) / 200 };
  return { min: 0, max: mag * 3 || 1, step: (mag * 3 || 1) / 200 };
}

function parseAnnotations(rawSource) {
  const ann = {};
  const re =
    /const\s+(?:int|float)\s+([A-Za-z_]\w*)\s*=\s*[^;]*;\s*\/\/[^\n]*?@knob\s+([^\n]+)/g;
  let m;
  while ((m = re.exec(rawSource))) {
    const name = m[1];
    const rest = m[2].trim();
    const labelMatch = rest.match(/"([^"]+)"/);
    const groupMatch = rest.match(/group:([A-Za-z]+)/);
    const nums = rest.replace(/"[^"]*"/, '').replace(/group:\S+/, '').trim().split(/\s+/).map(Number).filter((x) => !Number.isNaN(x));
    ann[name] = {
      min: nums[0],
      max: nums[1],
      step: nums[2],
      label: labelMatch ? labelMatch[1] : null,
      group: groupMatch ? groupMatch[1] : null,
    };
  }
  return ann;
}

// consts: from transpile().consts ; rawSource: original .ino text.
// Returns [{ group, knobs: [{name, label, type, value, min, max, step}] }]
export function buildKnobs(consts, rawSource) {
  const ann = parseAnnotations(rawSource);
  const groups = {};

  for (const c of consts) {
    if (!c.editable || HIDDEN.has(c.name)) continue;
    const a = ann[c.name] || {};
    const r = heuristicRange(c.name, c.number, c.type);
    const min = a.min != null ? a.min : r.min;
    const max = a.max != null ? a.max : r.max;
    const step = a.step != null ? a.step : (c.type === 'int' ? 1 : r.step);
    const group = a.group || classify(c.name);
    const label = a.label || prettyName(c.name);
    (groups[group] ||= []).push({
      name: c.name,
      label,
      type: c.type,
      value: c.number,
      min: Math.min(min, c.number),
      max: Math.max(max, c.number),
      step: step || 0.01,
    });
  }

  return GROUP_ORDER.filter((g) => groups[g]).map((g) => ({ group: g, knobs: groups[g] }));
}

function prettyName(name) {
  return name
    .toLowerCase()
    .replace(/_/g, ' ')
    .replace(/\bhz\b/, 'rate (Hz)')
    .replace(/\b\w/, (c) => c.toUpperCase());
}
