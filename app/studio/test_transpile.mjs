// Headless proof: transpile every pattern .ino, run frames, validate output.
import { readFileSync, readdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { transpile, serialize } from './transpile.js';

const here = dirname(fileURLToPath(import.meta.url));
const patternsDir = join(here, '..', 'patterns');

const dirs = readdirSync(patternsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory() && d.name !== 'js' && d.name !== 'dumb_player')
  .map((d) => d.name)
  .sort();

let pass = 0;
let fail = 0;
const problems = [];

for (const name of dirs) {
  const file = join(patternsDir, name, `${name}.ino`);
  let src;
  try {
    src = readFileSync(file, 'utf8');
  } catch {
    continue;
  }

  const r = transpile(src);
  if (!r.ok) {
    fail++;
    problems.push(`${name}: TRANSPILE FAIL — ${r.error}`);
    continue;
  }

  // run 60 frames over 2 simulated seconds, plus a later window for slow cycles.
  const out = new Array(30).fill(0);
  let allFinite = true;
  let anyLit = false;
  let everLit = false;
  const S = r.makeState();
  const times = [];
  for (let f = 0; f < 300; f++) times.push(f * 0.05); // 0..15s
  let frameErr = null;
  for (const t of times) {
    out.fill(0);
    try {
      r.step(t, 0.05, paramDefaults(r), S, out);
    } catch (e) {
      frameErr = e.message;
      break;
    }
    let litThisFrame = false;
    for (let i = 0; i < 30; i++) {
      const v = out[i];
      if (!Number.isFinite(v)) allFinite = false;
      if (v < 0 || v > 255) allFinite = false;
      if (v > 0) litThisFrame = true;
    }
    if (litThisFrame) everLit = true;
  }

  if (frameErr) {
    fail++;
    problems.push(`${name}: RUNTIME FAIL — ${frameErr}`);
    continue;
  }
  if (!allFinite) {
    fail++;
    problems.push(`${name}: produced NaN / out-of-range bytes`);
    continue;
  }
  if (!everLit) {
    fail++;
    problems.push(`${name}: never lit any LED across 15s (logic likely mistranspiled)`);
    continue;
  }

  // serialize round-trip: tweak first editable const, ensure it appears.
  const editable = r.consts.filter((c) => c.editable && c.name !== 'NUM_LEDS');
  let serialOk = true;
  if (editable.length) {
    const c = editable[0];
    const newVal = c.number + (c.type === 'int' ? 1 : 0.5);
    const re = serialize(src, { [c.name]: newVal });
    const reparsed = transpile(re);
    const back = reparsed.consts.find((x) => x.name === c.name);
    if (!back || Math.abs(back.number - newVal) > 1e-6) serialOk = false;
  }
  if (!serialOk) {
    fail++;
    problems.push(`${name}: serialize round-trip failed for ${editable[0]?.name}`);
    continue;
  }

  pass++;
  const knobs = r.consts.filter((c) => c.editable).map((c) => c.name).length;
  const stateful = r.states.length ? ` [stateful: ${r.states.map((s) => s.name).join(',')}]` : '';
  console.log(`  ok  ${name.padEnd(16)} ${knobs} knobs, ${r.arrays.length} arrays${stateful}`);
}

function paramDefaults(r) {
  const P = {};
  for (const c of r.consts) if (c.editable) P[c.name] = c.number;
  return P;
}

console.log(`\n${pass}/${pass + fail} patterns transpiled + ran clean.`);
if (problems.length) {
  console.log('\nPROBLEMS:');
  for (const p of problems) console.log('  ✗ ' + p);
  process.exit(1);
}
