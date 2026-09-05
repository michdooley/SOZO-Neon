// Bump Cascade — each of the 6 bumps lights as a pulse, staggered.
// `overlap` is the headline knob: 0 = no overlap (one bump finishes
// before the next starts), 1 = total overlap (all bumps fire together).

import { BUMP_STARTS, BUMP_ENDS, BUMP_PEAKS } from '../../editor/coords.js';
import { curves, curvesCpp, CURVE_NAMES } from '../../editor/curves.js';

const N_BUMPS = 6;

export default {
  id: 'bump_cascade',
  name: 'Bump Cascade',
  params: {
    pulseDur:   { type: 'float', min: 0.2, max: 3.0,  default: 1.0, step: 0.05, label: 'Pulse duration (s)' },
    overlap:    { type: 'float', min: 0,   max: 0.95, default: 0.6, step: 0.01, label: 'Overlap (0–1)' },
    curve:      { type: 'enum',  options: CURVE_NAMES, default: 'sineHump', label: 'Pulse curve' },
    peakSpread: { type: 'float', min: 0.5, max: 1.5,  default: 1.0, step: 0.05, label: 'Peak falloff width' },
    rest:       { type: 'float', min: 0,   max: 3,    default: 0.7, step: 0.05, label: 'Rest pause (s)' },
  },

  render(t, p, out) {
    const stagger = p.pulseDur * (1 - p.overlap);
    const cyclePeriod = (N_BUMPS - 1) * stagger + p.pulseDur + p.rest;
    const cycleT = ((t % cyclePeriod) + cyclePeriod) % cyclePeriod;
    const curve = curves[p.curve] || curves.sineHump;

    for (let bp = 0; bp < N_BUMPS; bp++) {
      const localT = cycleT - bp * stagger;
      if (localT < 0 || localT > p.pulseDur) continue;
      const amp = curve(localT / p.pulseDur);
      const peak = BUMP_PEAKS[bp];
      const rUp = peak - BUMP_STARTS[bp];
      const rDn = BUMP_ENDS[bp] - peak;
      const span = (Math.max(rUp, rDn) + 1) * p.peakSpread;
      for (let i = BUMP_STARTS[bp]; i <= BUMP_ENDS[bp]; i++) {
        const d = Math.abs(i - peak);
        const falloff = Math.max(0, 1 - d / span);
        const b = Math.floor(amp * falloff * 255);
        if (b > out[i]) out[i] = b;
      }
    }
  },

  toArduino(p) {
    const curveExpr = (curvesCpp[p.curve] || curvesCpp.sineHump)('u');
    return `// Generated from bump_cascade.js
const int   NUM_LEDS = 30;
const int   N_BUMPS  = 6;

const int STARTS[N_BUMPS] = { 0,  6, 11, 15, 20, 25 };
const int ENDS[N_BUMPS]   = { 5, 10, 14, 19, 24, 29 };
const int PEAKS[N_BUMPS]  = { 3,  8, 13, 19, 23, 27 };

const float PULSE_DUR   = ${p.pulseDur.toFixed(3)}f;
const float OVERLAP     = ${p.overlap.toFixed(3)}f;
const float PEAK_SPREAD = ${p.peakSpread.toFixed(3)}f;
const float REST        = ${p.rest.toFixed(3)}f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float stagger = PULSE_DUR * (1.0f - OVERLAP);
  float cyclePeriod = (N_BUMPS - 1) * stagger + PULSE_DUR + REST;
  float cycleT = fmod(t, cyclePeriod);

  uint8_t buf[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) buf[i] = 0;

  for (int bp = 0; bp < N_BUMPS; bp++) {
    float localT = cycleT - bp * stagger;
    if (localT < 0.0f || localT > PULSE_DUR) continue;
    float u = localT / PULSE_DUR;
    float amp = ${curveExpr};

    int rUp = PEAKS[bp] - STARTS[bp];
    int rDn = ENDS[bp]  - PEAKS[bp];
    int radius = (rUp > rDn) ? rUp : rDn;
    float span = ((float)radius + 1.0f) * PEAK_SPREAD;

    for (int i = STARTS[bp]; i <= ENDS[bp]; i++) {
      float d = fabs((float)(i - PEAKS[bp]));
      float falloff = 1.0f - d / span;
      if (falloff < 0.0f) falloff = 0.0f;
      int b = (int)(amp * falloff * 255.0f);
      if (b > buf[i]) buf[i] = b;
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    Serial.print(buf[i]);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\\n');
  }
  delay(33);
}
`;
  },
};
