// jsc harness: prove each directional reverse (a) stays valid and (b) actually
// flips propagation direction — wrap-proof, using a first-harmonic phase
// (for index/x sweeps) or centroid radius (for radial sweeps), regressed over
// the active window. Assumes transpile()/serialize()/DIRECTIONAL/applyReverse()
// defined above (concatenated by run_reverse.sh). arguments[0] = patterns dir.

const PAT = arguments[0];
const XS = [
  -21.82, -19.58, -16.45, -12.63, -8.33, -5.46, -3.83, -1.55, 1.35, 4.93,
  6.81, 7.83, 10.10, 13.25, 15.54,
  -3.29, -2.05, -0.19, 2.37, 6.00, 8.87, 9.38, 10.58, 13.32, 16.13,
  17.72, 18.20, 19.33, 21.75, 24.16];
const YS = [
  -9.52, -2.56, 3.06, 5.97, 4.54, 1.21, 5.94, 10.24, 12.83, 12.26,
  9.21, 13.34, 16.27, 16.84, 14.94,
  -14.01, -7.86, -2.64, 0.63, 0.93, -1.96, 3.13, 7.04, 9.20, 8.89,
  6.81, 10.66, 13.49, 14.37, 13.00];
const OX = 18.20, OY = 10.66;

// per-frame metric: returns {m, lit}. index/x -> harmonic phase angle; radius -> centroid r.
function metric(out, axis) {
  let den = 0;
  for (let i = 0; i < 30; i++) den += out[i];
  if (den < 1) return { m: null, lit: false };
  if (axis === 'radius') {
    let num = 0;
    for (let i = 0; i < 30; i++) {
      const dx = XS[i] - OX, dy = YS[i] - OY;
      num += Math.sqrt(dx * dx + dy * dy) * out[i];
    }
    return { m: num / den, lit: true };
  }
  let re = 0, im = 0;
  for (let i = 0; i < 30; i++) {
    const phi = axis === 'index'
      ? (2 * Math.PI * i) / 30
      : (2 * Math.PI * (XS[i] + 28)) / 56;
    re += out[i] * Math.cos(phi);
    im += out[i] * Math.sin(phi);
  }
  return { m: Math.atan2(im, re), lit: true };
}

// run 0..2.6s, collect (t, metric); unwrap angular metrics; return least-squares slope.
function slope(compiled, params, axis) {
  const out = new Array(30).fill(0);
  const S = compiled.makeState(params);
  const ts = [], ms = [];
  let prev = null;
  for (let f = 0; f <= 52; f++) {
    const t = f * 0.05;
    out.fill(0);
    compiled.step(t, 0.05, params, S, out);
    if (t < 0.3) continue;
    const r = metric(out, axis);
    if (!r.lit) continue;
    let m = r.m;
    if (axis !== 'radius' && prev !== null) {
      while (m - prev > Math.PI) m -= 2 * Math.PI;
      while (m - prev < -Math.PI) m += 2 * Math.PI;
    }
    prev = m;
    ts.push(t); ms.push(m);
  }
  if (ts.length < 5) return null;
  const n = ts.length;
  let st = 0, sm = 0;
  for (let i = 0; i < n; i++) { st += ts[i]; sm += ms[i]; }
  const mt = st / n, mm = sm / n;
  let cov = 0, vr = 0;
  for (let i = 0; i < n; i++) { cov += (ts[i] - mt) * (ms[i] - mm); vr += (ts[i] - mt) ** 2; }
  return vr > 0 ? cov / vr : null;
}

function runValid(compiled, params) {
  const out = new Array(30).fill(0);
  const S = compiled.makeState(params);
  let everLit = false;
  for (let f = 0; f < 240; f++) {
    out.fill(0);
    try { compiled.step(f * 0.05, 0.05, params, S, out); } catch (e) { return 'threw: ' + e.message; }
    for (let i = 0; i < 30; i++) {
      const v = out[i];
      if (!isFinite(v) || v < 0 || v > 255) return 'invalid byte';
      if (v > 0) everLit = true;
    }
  }
  return everLit ? null : 'never lit';
}

let fail = 0;
for (const id of Object.keys(DIRECTIONAL)) {
  const spec = DIRECTIONAL[id];
  const src = readFile(PAT + '/' + id + '/' + id + '.ino');
  const fwd = transpile(src);
  const P = {};
  for (const c of fwd.consts) if (c.editable) P[c.name] = c.number;

  const rev = applyReverse(spec, src, P);
  const revC = spec.source ? transpile(rev.source) : fwd;
  const revP = rev.params;

  if (!revC.ok) { print('  x ' + pad(id, 14) + 'reverse transpile FAIL: ' + revC.error); fail++; continue; }
  const vErr = runValid(revC, revP);
  if (vErr) { print('  x ' + pad(id, 14) + 'reverse invalid: ' + vErr); fail++; continue; }

  if (spec.negate) { print('  ok  ' + pad(id, 14) + 'valid (sign-flip; reversal is exact)'); continue; }

  const sf = slope(fwd, P, spec.axis);
  const sr = slope(revC, revP, spec.axis);
  if (sf == null || sr == null) { print('  x ' + pad(id, 14) + 'could not measure direction'); fail++; continue; }
  if (!(Math.sign(sf) === -Math.sign(sr) && Math.abs(sf) > 1e-3 && Math.abs(sr) > 1e-3)) {
    print('  x ' + pad(id, 14) + 'did NOT flip (fwd ' + fmt(sf) + ', rev ' + fmt(sr) + ')'); fail++; continue;
  }
  print('  ok  ' + pad(id, 14) + 'flips (fwd ' + (sf > 0 ? '+' : '-') + ', rev ' + (sr > 0 ? '+' : '-') + ' along ' + spec.axis + ')');
}
function pad(s, n) { while (s.length < n) s += ' '; return s; }
function fmt(x) { return Math.round(x * 1000) / 1000; }
print(fail ? '\nRESULT: FAIL' : '\nRESULT: PASS');
