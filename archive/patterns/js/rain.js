// Rain — bright droplets spawn at the top, fall under gravity, despawn at
// the bottom. Stateful pattern: `state.drops` is an array of particles
// carried across frames. Reset clears the array.

import { XS, YS, XS_INO, YS_INO } from '../../editor/coords.js';

const X_LO = -20, X_HI = 22, Y_TOP = 22, Y_BOT = -20;
const MAX_DROPS = 12;

export default {
  id: 'rain',
  name: 'Rain',
  params: {
    spawnRate: { type: 'float', min: 0.5, max: 12,  default: 3,   step: 0.1, label: 'Spawn rate (drops/s)' },
    gravity:   { type: 'float', min: 5,   max: 100, default: 35,  step: 1,   label: 'Gravity (in/s²)' },
    sigma:     { type: 'float', min: 0.5, max: 5,   default: 1.6, step: 0.1, label: 'Drop size σ (in)' },
    vy0:       { type: 'float', min: -8,  max: 0,   default: -2,  step: 0.1, label: 'Initial velocity (in/s)' },
  },

  init() {
    return { drops: [] };
  },

  render(t, p, out, state, dt) {
    if (Math.random() < p.spawnRate * dt && state.drops.length < MAX_DROPS) {
      state.drops.push({
        x: X_LO + Math.random() * (X_HI - X_LO),
        y: Y_TOP,
        vy: p.vy0,
      });
    }
    for (const d of state.drops) {
      d.vy -= p.gravity * dt;
      d.y  += d.vy * dt;
    }
    state.drops = state.drops.filter(d => d.y > Y_BOT);

    const twoSig2 = 2 * p.sigma * p.sigma;
    for (let i = 0; i < 30; i++) {
      let max = 0;
      for (const d of state.drops) {
        const dx = XS[i] - d.x;
        const dy = YS[i] - d.y;
        const g = Math.exp(-(dx * dx + dy * dy) / twoSig2);
        if (g > max) max = g;
      }
      out[i] = Math.floor(max * 255);
    }
  },

  toArduino(p) {
    return `// Generated from rain.js
const int NUM_LEDS = 30;

${XS_INO}
${YS_INO}

const int   MAX_DROPS  = 8;
const float SPAWN_RATE = ${p.spawnRate.toFixed(3)}f;
const float G_ACC      = ${p.gravity.toFixed(2)}f;
const float DROP_SIGMA = ${p.sigma.toFixed(3)}f;
const float VY0        = ${p.vy0.toFixed(3)}f;
const float X_LO       = -20.0f;
const float X_HI       =  22.0f;
const float Y_TOP      =  22.0f;
const float Y_BOT      = -20.0f;

struct Drop { float x; float y; float vy; bool active; };
Drop drops[MAX_DROPS];

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));
  for (int i = 0; i < MAX_DROPS; i++) drops[i].active = false;
}

void loop() {
  const float dt = 0.033f;
  if (random(1000) < (long)(SPAWN_RATE * 33.0f)) {
    for (int s = 0; s < MAX_DROPS; s++) {
      if (!drops[s].active) {
        drops[s].x  = X_LO + (random(1000) / 1000.0f) * (X_HI - X_LO);
        drops[s].y  = Y_TOP;
        drops[s].vy = VY0;
        drops[s].active = true;
        break;
      }
    }
  }
  for (int s = 0; s < MAX_DROPS; s++) {
    if (!drops[s].active) continue;
    drops[s].vy -= G_ACC * dt;
    drops[s].y  += drops[s].vy * dt;
    if (drops[s].y < Y_BOT) drops[s].active = false;
  }
  float twoSigSq = 2.0f * DROP_SIGMA * DROP_SIGMA;
  for (int i = 0; i < NUM_LEDS; i++) {
    float bright = 0.0f;
    for (int s = 0; s < MAX_DROPS; s++) {
      if (!drops[s].active) continue;
      float dx = XS[i] - drops[s].x;
      float dy = YS[i] - drops[s].y;
      float d2 = dx * dx + dy * dy;
      float g  = exp(-d2 / twoSigSq);
      if (g > bright) bright = g;
    }
    int b = (int)(bright * 255.0f);
    Serial.print(b);
    Serial.print(i < NUM_LEDS - 1 ? ',' : '\\n');
  }
  delay(33);
}
`;
  },
};
