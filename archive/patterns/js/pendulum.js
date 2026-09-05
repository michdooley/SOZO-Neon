// Pendulum — a gaussian blob swings horizontally with sinusoidal motion
// (slow at extremes, fast through the middle).

import { XS, YS, XS_INO, YS_INO } from '../../editor/coords.js';

export default {
  id: 'pendulum',
  name: 'Pendulum',
  params: {
    swingHz:  { type: 'float', min: 0.05, max: 2.0, default: 0.35, step: 0.01, label: 'Swing rate (Hz)' },
    swingAmp: { type: 'float', min: 5,    max: 26,  default: 22,   step: 0.5,  label: 'Swing amp (in)' },
    sigma:    { type: 'float', min: 1,    max: 15,  default: 5.5,  step: 0.1,  label: 'Blob size σ (in)' },
    cy:       { type: 'float', min: -10,  max: 18,  default: 5,    step: 0.5,  label: 'Vertical center (in)' },
  },

  render(t, p, out) {
    const cx = p.swingAmp * Math.sin(t * 2 * Math.PI * p.swingHz);
    const twoSig2 = 2 * p.sigma * p.sigma;
    for (let i = 0; i < 30; i++) {
      const dx = XS[i] - cx;
      const dy = YS[i] - p.cy;
      const g = Math.exp(-(dx * dx + dy * dy) / twoSig2);
      out[i] = Math.floor(g * 255);
    }
  },

  toArduino(p) {
    return `// Generated from pendulum.js
const int NUM_LEDS = 30;

${XS_INO}
${YS_INO}

const float SWING_HZ  = ${p.swingHz.toFixed(3)}f;
const float SWING_AMP = ${p.swingAmp.toFixed(2)}f;
const float SIGMA     = ${p.sigma.toFixed(2)}f;
const float CY        = ${p.cy.toFixed(2)}f;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  float cx = SWING_AMP * sin(t * 2.0f * PI * SWING_HZ);
  float twoSigSq = 2.0f * SIGMA * SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = XS[i] - cx;
    float dy = YS[i] - CY;
    float d2 = dx * dx + dy * dy;
    float g  = exp(-d2 / twoSigSq);
    int b = (int)(g * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\\n');
  }
  delay(33);
}
`;
  },
};
