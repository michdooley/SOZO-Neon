// jsc harness for knobs.js + serialize integration. Assumes transpile(),
// serialize(), buildKnobs() already defined above (concatenated by run_knobs.sh).
function baseName(p) { const a = p.split('/'); return a[a.length - 1].replace('.ino', ''); }

let fail = 0;
for (let a = 0; a < arguments.length; a++) {
  const file = arguments[a];
  const name = baseName(file);
  const src = readFile(file);
  const r = transpile(src);
  let groups;
  try { groups = buildKnobs(r.consts, src); }
  catch (e) { print('  x ' + name + ': buildKnobs threw — ' + e.message); fail++; continue; }

  const total = groups.reduce((n, g) => n + g.knobs.length, 0);
  // every knob's default must sit within [min,max]
  let bad = '';
  for (const g of groups) for (const k of g.knobs) {
    if (!(k.value >= k.min && k.value <= k.max)) bad = k.name + ' (' + k.value + ' not in [' + k.min + ',' + k.max + '])';
    if (!(k.step > 0)) bad = k.name + ' bad step';
  }
  if (bad) { print('  x ' + name + ': ' + bad); fail++; continue; }

  print('  ok  ' + pad(name, 16) + total + ' knobs in [' + groups.map((g) => g.group).join(', ') + ']');
}
function pad(s, n) { while (s.length < n) s += ' '; return s; }
print(fail ? '\nRESULT: FAIL' : '\nRESULT: PASS');
