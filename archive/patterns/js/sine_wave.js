// Sine Wave — brightness = sin(k * world_x + ω * t), projected onto each
// LED's actual x-coordinate. The wave physically rolls across the board.

import { XS, XS_INO } from '../../editor/coords.js';

export default {
  id: 'sine_wave',
  name: 'Sine Wave',
  params: {
    k:     { type: 'float', min: 0.05, max: 1.5, default: 0.35, step: 0.01, label: 'Wavenumber (rad/in)' },
    omega: { type: 'float', min: -10,  max: 10,  default: 3.0,  step: 0.1,  label: 'Speed (rad/s)' },
    floor: { type: 'float', min: 0,    max: 0.5, default: 0,    step: 0.01, label: 'Brightness floor' },
  },

  render(t, p, out) {
    const range = 1 - p.floor;
    for (let i = 0; i < 30; i++) {
      const phase = p.k * XS[i] + p.omega * t;
      const v = (Math.sin(phase) * 0.5 + 0.5) * range + p.floor;
      out[i] = Math.floor(v * 255);
    }
  },

  toArduino(p) {
    return `// Generated from sine_wave.js
const int NUM_LEDS = 30;

${XS_INO}

const float K     = ${p.k.toFixed(4)}f;
const float OMEGA = ${p.omega.toFixed(3)}f;
const float FLOOR = ${p.floor.toFixed(3)}f;
const float RANGE = 1.0f - FLOOR;

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = millis() / 1000.0f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float phase = K * XS[i] + OMEGA * t;
    float v = (sin(phase) * 0.5f + 0.5f) * RANGE + FLOOR;
    int b = (int)(v * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\\n');
  }
  delay(33);
}
`;
  },
};
