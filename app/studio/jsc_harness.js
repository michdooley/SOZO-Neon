// jsc test harness — assumes transpile() + serialize() already defined above
// (concatenated by run_test.sh). Pattern .ino paths come in via `arguments`.

function paramDefaults(r) {
  const P = {};
  for (const c of r.consts) if (c.editable) P[c.name] = c.number;
  return P;
}

function baseName(p) {
  const parts = p.split('/');
  return parts[parts.length - 1].replace('.ino', '');
}

let pass = 0, fail = 0;
const problems = [];

for (let a = 0; a < arguments.length; a++) {
  const file = arguments[a];
  const name = baseName(file);
  let src;
  try { src = readFile(file); } catch (e) { problems.push(name + ': cannot read'); fail++; continue; }

  const r = transpile(src);
  if (!r.ok) { fail++; problems.push(name + ': TRANSPILE FAIL — ' + r.error); continue; }

  const out = new Array(30).fill(0);
  let allFinite = true, everLit = false, frameErr = null;
  const S = r.makeState();
  const P = paramDefaults(r);
  for (let f = 0; f < 300; f++) {
    const t = f * 0.05;
    out.fill(0);
    try { r.step(t, 0.05, P, S, out); }
    catch (e) { frameErr = e.message; break; }
    for (let i = 0; i < 30; i++) {
      const v = out[i];
      if (!isFinite(v) || v < 0 || v > 255) allFinite = false;
      if (v > 0) everLit = true;
    }
  }

  if (frameErr) { fail++; problems.push(name + ': RUNTIME FAIL — ' + frameErr); continue; }
  if (!allFinite) { fail++; problems.push(name + ': NaN / out-of-range bytes'); continue; }
  if (!everLit) { fail++; problems.push(name + ': never lit any LED across 15s'); continue; }

  // serialize round-trip
  const editable = r.consts.filter((c) => c.editable && c.name !== 'NUM_LEDS');
  let serialOk = true, badName = '';
  if (editable.length) {
    const c = editable[0];
    const newVal = c.number + (c.type === 'int' ? 1 : 0.5);
    const re = serialize(src, { [c.name]: newVal });
    const reparsed = transpile(re);
    const back = reparsed.consts.find((x) => x.name === c.name);
    if (!back || Math.abs(back.number - newVal) > 1e-6) { serialOk = false; badName = c.name; }
  }
  if (!serialOk) { fail++; problems.push(name + ': serialize round-trip failed for ' + badName); continue; }

  pass++;
  const knobs = r.consts.filter((c) => c.editable).length;
  const stateful = r.states.length ? '  [state: ' + r.states.map((s) => s.name).join(',') + ']' : '';
  print('  ok  ' + pad(name, 16) + ' ' + knobs + ' knobs, ' + r.arrays.length + ' arrays' + stateful);
}

function pad(s, n) { while (s.length < n) s += ' '; return s; }

print('\n' + pass + '/' + (pass + fail) + ' patterns transpiled + ran clean.');
if (problems.length) {
  print('\nPROBLEMS:');
  for (let i = 0; i < problems.length; i++) print('  x ' + problems[i]);
  print('\nRESULT: FAIL');
} else {
  print('\nRESULT: PASS');
}
