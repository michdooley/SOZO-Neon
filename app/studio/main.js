// SOZO Pattern Studio — controller.
//
// The .ino files in ../patterns are the source of truth. Selecting a
// pattern fetches its sketch, transpiles loop() to a JS step function
// for the live 3D preview, lifts its const block into sliders, and lets
// you re-export a tuned .ino. No per-pattern JS is maintained anywhere.

import { initScene } from '../editor/scene.js';
import { transpile, serialize } from './transpile.js';
import { buildKnobs } from './knobs.js';
import { PATTERNS, PRETTY } from './manifest.js';
import { DIRECTIONAL, applyReverse } from './directional.js';
import * as reviews from './reviews.js';

const previewEl = document.getElementById('preview');
const { setLed, setAll } = initScene(previewEl);

const listEl = document.getElementById('patternList');
const knobsEl = document.getElementById('knobs');
const reviewEl = document.getElementById('review');
const dimEl = document.getElementById('globalDim');
const descEl = document.getElementById('desc');
const statusEl = document.getElementById('status');
const resetBtn = document.getElementById('reset');
const revertBtn = document.getElementById('revert');
const exportBtn = document.getElementById('export');
const downloadBtn = document.getElementById('download');
const duplicateBtn = document.getElementById('duplicate');
const renameBtn = document.getElementById('rename');
const deleteBtn = document.getElementById('delete');
const connectBtn = document.getElementById('connect');
const toggleSrcBtn = document.getElementById('toggleSrc');
const srcWrap = document.getElementById('srcWrap');
const srcEl = document.getElementById('src');
const srcNameEl = document.getElementById('srcName');
const copyBtn = document.getElementById('copySrc');

const out = new Uint8Array(30);

let current = null; // { id, source, compiled, params }
let state = {};
let clockStart = performance.now();
let lastT = 0;

const setStatus = (s) => { statusEl.textContent = s; };

// ---------- per-pattern tuning cache ----------
// Remembers the values you've tuned, per pattern id, so selecting another
// pattern and coming back doesn't reset your sliders to the file defaults.
// Stored as diffs from the file defaults (+ the reverse flag) and persisted to
// localStorage so it also survives a page reload. Export still bakes the final
// values into the .ino; this is just the live working state.
const TUNING_KEY = 'sozo_studio_tuning';
let tuning = {};
try { tuning = JSON.parse(localStorage.getItem(TUNING_KEY)) || {}; } catch { tuning = {}; }
function saveTuning() { try { localStorage.setItem(TUNING_KEY, JSON.stringify(tuning)); } catch { /* ignore */ } }

function rememberTuning() {
  if (!current) return;
  const diffs = {};
  for (const c of current.compiled.consts) {
    if (c.editable && current.params[c.name] !== c.number) diffs[c.name] = current.params[c.name];
  }
  if (Object.keys(diffs).length || current.reversed) {
    tuning[current.id] = { params: diffs, reversed: !!current.reversed };
  } else {
    delete tuning[current.id];   // back to all file defaults -> no entry needed
  }
  saveTuning();
}

// ---------- pattern list + reviews ----------
// patternIds is dynamic: the server's live scan of patterns/ (built-ins +
// studio-created variants), falling back to the static manifest. buildList()
// (re)renders the left list from it after any add/rename/delete.
let patternIds = [...PATTERNS];
const liById = new Map();

// The SozoBasic gallery, in firmware GAL_PATTERNS order. These render as a
// numbered "folder" with a ▶ Play control that runs the whole sequence — fade
// in, hold, fade out, next, loop — just like the firmware's gallery stage.
const GALLERY_IDS = [...PATTERNS];
const collapsed = new Set();

// Gallery-player state (defined here because buildList() reads it at load time;
// the player's functions live further down, near the render loop). Firmware runs
// 120 s per pattern with a 5 s fade; the preview defaults are shorter so it's
// watchable — both are adjustable in the folder header.
const GAL_KEY = 'sozo_studio_gallery';
const gallery = { active: false, clips: [], idx: 0, clipStart: 0, lastNow: 0, holdSec: 8, fadeSec: 1.5 };
(function loadGalleryCfg() {
  try { Object.assign(gallery, JSON.parse(localStorage.getItem(GAL_KEY)) || {}); } catch { /* ignore */ }
  gallery.active = false; gallery.clips = []; gallery.idx = 0;   // never persist runtime state
})();

function makeMemberLi(id, number) {
  const li = document.createElement('li');
  li.dataset.id = id;
  li.className = 'in-group';
  li.classList.toggle('active', current && current.id === id);
  if (number != null) {
    const num = document.createElement('span');
    num.className = 'pnum';
    num.textContent = String(number).padStart(2, '0');
    li.appendChild(num);
  }
  const name = document.createElement('span');
  name.className = 'pname';
  name.textContent = PRETTY(id);
  const badge = document.createElement('span');
  badge.className = 'badge';
  li.append(name, badge);
  li.addEventListener('click', () => selectPattern(id));
  listEl.appendChild(li);
  liById.set(id, { li, badge });
  updateBadge(id);
}

function makeGroupHeader(key, title, count, withPlay) {
  const li = document.createElement('li');
  li.className = 'group-header';
  const caret = document.createElement('span');
  caret.className = 'caret';
  caret.textContent = collapsed.has(key) ? '▸' : '▾';
  const label = document.createElement('span');
  label.className = 'gname';
  label.textContent = title;
  const cnt = document.createElement('span');
  cnt.className = 'gcount';
  cnt.textContent = String(count);
  li.append(caret, label, cnt);
  if (withPlay) {
    const play = document.createElement('button');
    play.className = 'play-btn';
    play.id = 'galleryPlay';
    play.textContent = gallery.active ? '⏹' : '▶';
    play.title = 'Play the whole gallery in sequence, like SozoBasic';
    play.addEventListener('click', (ev) => { ev.stopPropagation(); toggleGallery(); });
    li.appendChild(play);
  }
  li.addEventListener('click', () => {
    if (collapsed.has(key)) collapsed.delete(key); else collapsed.add(key);
    buildList();
  });
  listEl.appendChild(li);
}

// Compact per-gallery timing controls (sec/pattern + fade). Firmware runs 120 s
// per pattern with a 5 s fade; the preview defaults are shorter so it's watchable.
function makeGalleryControls() {
  const li = document.createElement('li');
  li.className = 'group-controls';
  const mk = (label, key, min, max, step) => {
    const w = document.createElement('label');
    w.className = 'gc';
    const s = document.createElement('span');
    s.textContent = label;
    const inp = document.createElement('input');
    inp.type = 'number';
    inp.min = min; inp.max = max; inp.step = step; inp.value = gallery[key];
    inp.addEventListener('click', (ev) => ev.stopPropagation());
    inp.addEventListener('change', () => {
      const v = parseFloat(inp.value);
      if (!Number.isNaN(v)) { gallery[key] = v; saveGalleryCfg(); }
    });
    w.append(s, inp);
    return w;
  };
  li.append(mk('sec / pattern', 'holdSec', 0.5, 300, 0.5), mk('fade (s)', 'fadeSec', 0, 30, 0.5));
  listEl.appendChild(li);
}

function buildList() {
  listEl.innerHTML = '';
  liById.clear();
  const inGallery = GALLERY_IDS.filter((id) => patternIds.includes(id));
  const extras = patternIds.filter((id) => !GALLERY_IDS.includes(id));

  if (inGallery.length) {
    makeGroupHeader('gallery', 'SozoBasic Gallery', inGallery.length, true);
    if (!collapsed.has('gallery')) {
      makeGalleryControls();
      inGallery.forEach((id, i) => makeMemberLi(id, i + 1));
    }
  }
  if (extras.length) {
    makeGroupHeader('workspace', 'Workspace', extras.length, false);
    if (!collapsed.has('workspace')) extras.forEach((id) => makeMemberLi(id, null));
  }
  applyFilter();
  highlightGallery();
}

// Pull the live pattern list from serve.py; keep built-ins in manifest order and
// append any extra (variant) ids alphabetically. Falls back to the static list.
async function loadPatternList() {
  try {
    const res = await fetch('patterns');
    if (res.ok) {
      const j = await res.json();
      const scanned = new Set(j.patterns || []);
      if (scanned.size) {
        const builtins = PATTERNS.filter((id) => scanned.has(id));
        const extras = [...scanned].filter((id) => !PATTERNS.includes(id)).sort();
        patternIds = [...builtins, ...extras];
      }
    }
  } catch { /* plain http.server / file:// — keep the static manifest */ }
  buildList();
}

function updateBadge(id) {
  const ref = liById.get(id);
  if (!ref) return;
  const r = reviews.getReview(id);
  ref.badge.innerHTML = '';
  const glyph = reviews.FLAG_GLYPH[r.flag];
  if (glyph) {
    const f = document.createElement('span');
    f.className = `bflag ${r.flag}`;
    f.textContent = glyph;
    ref.badge.appendChild(f);
  }
  if (r.rating) {
    const s = document.createElement('span');
    s.className = 'brate';
    s.textContent = `★${r.rating}`;
    ref.badge.appendChild(s);
  }
}

function applyFilter() {
  let shown = 0;
  let rendered = 0;
  for (const id of patternIds) {
    const ref = liById.get(id);
    if (!ref) continue;              // member lives in a collapsed group
    rendered++;
    const vis = reviews.passesFilter(id);
    ref.li.style.display = vis ? '' : 'none';
    if (vis) shown++;
  }
  filterCountEl && (filterCountEl.textContent = shown < rendered ? `${shown}/${rendered} shown` : '');
}

reviews.loadReviews();
reviews.renderFilter(document.getElementById('filter'), applyFilter);
const filterCountEl = document.createElement('div');
filterCountEl.style.cssText = 'font-size:10px;color:#666;margin-top:2px;';
document.getElementById('filter').appendChild(filterCountEl);
buildList();
// If localStorage was empty, pull any committed reviews.json (served by serve.py).
reviews.seedFromServer(new URL('./reviews.json', import.meta.url)).then((seeded) => {
  if (!seeded) return;
  for (const id of patternIds) updateBadge(id);
  applyFilter();
  if (current) reviews.renderEditor(reviewEl, current.id, () => { updateBadge(current.id); applyFilter(); });
});

async function selectPattern(id) {
  if (gallery.active) { gallery.active = false; highlightGallery(); }
  for (const li of listEl.children) li.classList.toggle('active', li.dataset.id === id);
  setStatus(`loading ${id}…`);
  let source;
  try {
    const url = new URL(`../patterns/${id}/${id}.ino`, import.meta.url);
    source = await (await fetch(url)).text();
  } catch (e) {
    setStatus(`could not load ${id}.ino: ${e.message}`);
    return;
  }

  const compiled = transpile(source);
  const params = {};
  for (const c of compiled.consts) if (c.editable) params[c.name] = c.number;

  // Restore any tuning you'd done on this pattern (cached as diffs from the
  // file defaults), so switching away and back doesn't reset your sliders.
  const cached = tuning[id];
  if (cached?.params) {
    for (const k in cached.params) if (k in params) params[k] = cached.params[k];
  }

  const spec = DIRECTIONAL[id] || null;
  const compiledRev = spec?.source ? transpile(spec.source(source)) : null;
  current = { id, source, compiled, params, spec, compiledRev, reversed: !!cached?.reversed };
  descEl.classList.remove('empty');
  descEl.textContent = firstDocLine(source) || PRETTY(id);

  reviews.renderEditor(reviewEl, id, () => { updateBadge(id); applyFilter(); });
  renderKnobs();
  resetState();
  refreshSource();
  exportBtn.disabled = false;
  downloadBtn.disabled = false;
  duplicateBtn.disabled = false;
  renameBtn.disabled = false;
  deleteBtn.disabled = false;

  if (!compiled.ok) {
    setStatus(`⚠ ${id}: preview unavailable (${compiled.error}). Edit + export still work.`);
  } else {
    setStatus(`${id} — ${countKnobs()} parameters · ${compiled.states.length ? 'stateful' : 'stateless'}`);
  }
}

function firstDocLine(src) {
  const m = src.match(/\/\/[^\n]*\n\/\/\s*([^\n]+)/); // 2nd comment line is usually the gloss
  return m ? m[1].trim() : null;
}
function countKnobs() {
  return current.compiled.consts.filter((c) => c.editable).length;
}

// Resolve the compiled step + params actually in effect (forward, or reversed
// via a structural variant or a sign-flipped param).
function effective() {
  const { spec, reversed } = current;
  if (reversed && spec) {
    if (spec.source && current.compiledRev?.ok) return { compiled: current.compiledRev, params: current.params };
    if (spec.negate) {
      const p = { ...current.params };
      for (const n of spec.negate) if (n in p) p[n] = -p[n];
      return { compiled: current.compiled, params: p };
    }
  }
  return { compiled: current.compiled, params: current.params };
}

// The .ino we'd export / show, with any reversal baked in.
function outputSource() {
  const r = current.reversed && current.spec
    ? applyReverse(current.spec, current.source, current.params)
    : { source: current.source, params: current.params };
  return serialize(r.source, r.params);
}

function resetState() {
  const e = effective();
  if (e.compiled?.ok) state = e.compiled.makeState(e.params);
  clockStart = performance.now();
  lastT = 0;
}

// ---------- knobs ----------
function renderKnobs() {
  knobsEl.innerHTML = '';
  if (current.spec) knobsEl.appendChild(makeReverseToggle());
  if (!current.compiled.consts.some((c) => c.editable)) {
    const p = document.createElement('p');
    p.className = 'empty';
    p.textContent = 'No tunable constants found in this sketch.';
    knobsEl.appendChild(p);
    return;
  }
  const groups = buildKnobs(current.compiled.consts, current.source);
  for (const { group, knobs } of groups) {
    const h = document.createElement('h2');
    h.textContent = group;
    knobsEl.appendChild(h);
    for (const k of knobs) knobsEl.appendChild(makeKnob(k));
  }
}

function makeReverseToggle() {
  const wrap = document.createElement('div');
  wrap.className = 'toggle';
  const h = document.createElement('h2');
  h.textContent = 'Direction';
  const row = document.createElement('label');
  row.className = 'toggle-row';
  const cb = document.createElement('input');
  cb.type = 'checkbox';
  cb.checked = current.reversed;
  const txt = document.createElement('span');
  txt.textContent = 'Reverse direction';
  cb.addEventListener('change', () => {
    current.reversed = cb.checked;
    resetState();
    rememberTuning();
    refreshSource();
  });
  row.append(cb, txt);
  wrap.append(h, row);
  return wrap;
}

function makeKnob(spec) {
  const wrap = document.createElement('div');
  wrap.className = 'knob';

  const label = document.createElement('div');
  label.className = 'knob-label';
  const nm = document.createElement('span');
  nm.className = 'name';
  nm.textContent = spec.label;
  label.appendChild(nm);
  wrap.appendChild(label);

  // Initialize from the live param (which may hold restored/tuned values), not
  // just the file default — and widen the range if the tuned value sits outside.
  const initVal = (current.params && spec.name in current.params) ? current.params[spec.name] : spec.value;
  const row = document.createElement('div');
  row.className = 'knob-row';
  const range = document.createElement('input');
  range.type = 'range';
  range.min = Math.min(spec.min, initVal); range.max = Math.max(spec.max, initVal); range.step = spec.step;
  range.value = initVal;
  const num = document.createElement('input');
  num.type = 'number';
  num.step = spec.step; num.value = round(initVal);

  const apply = (v, structural) => {
    current.params[spec.name] = v;
    if (structural || /^(N_|MAX_)/.test(spec.name)) resetState();
    rememberTuning();
    refreshSource();
  };
  range.addEventListener('input', () => {
    const v = parseFloat(range.value);
    num.value = round(v);
    apply(v);
  });
  num.addEventListener('change', () => {
    let v = parseFloat(num.value);
    if (Number.isNaN(v)) return;
    range.value = v;
    apply(v, true);
  });

  row.append(range, num);
  wrap.appendChild(row);
  return wrap;
}

const round = (v) => (Math.abs(v) >= 100 || Number.isInteger(v) ? String(Math.round(v)) : (+v).toPrecision(3).replace(/\.?0+$/, ''));

// ---------- live source panel ----------
function refreshSource() {
  if (!current) return;
  const tuned = outputSource();
  current.tuned = tuned;
  srcEl.textContent = tuned;
  srcNameEl.textContent = `${current.id}.ino${current.reversed ? '  (reversed)' : ''}`;
}

toggleSrcBtn.addEventListener('click', () => {
  srcWrap.classList.toggle('open');
  toggleSrcBtn.classList.toggle('active', srcWrap.classList.contains('open'));
});
copyBtn.addEventListener('click', async () => {
  try { await navigator.clipboard.writeText(current.tuned); copyBtn.textContent = 'copied'; setTimeout(() => (copyBtn.textContent = 'copy'), 1200); }
  catch { copyBtn.textContent = 'copy failed'; }
});

// ---------- actions ----------
resetBtn.addEventListener('click', resetState);

// Drop the cached tuning for this pattern and reload its file defaults.
revertBtn.addEventListener('click', () => {
  if (!current) return;
  const id = current.id;
  delete tuning[id];
  saveTuning();
  selectPattern(id);   // re-fetches from disk; cache is now empty
  setStatus(`reverted ${id} to file defaults`);
});

// Trigger a browser download of the current tuned .ino. Returns the filename.
function downloadIno() {
  if (!current) return null;
  const name = `${current.id}.ino`;
  const content = outputSource();
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([content], { type: 'text/plain' }));
  a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  URL.revokeObjectURL(a.href);
  return name;
}

exportBtn.addEventListener('click', async () => {
  if (!current) return;
  const name = `${current.id}.ino`;
  const content = outputSource();
  // Prefer writing into refined-patterns/ via serve.py; fall back to download.
  try {
    const res = await fetch('save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, content }),
    });
    if (res.ok) {
      const j = await res.json();
      setStatus(`✓ wrote ${j.path} (overwritten) — originals in patterns/ untouched`);
      return;
    }
  } catch { /* serve.py not running — download instead */ }

  downloadIno();
  setStatus(`downloaded ${name} — run "python3 studio/serve.py" to write refined-patterns/ directly`);
});

// Always-download backup, regardless of whether serve.py is running.
downloadBtn.addEventListener('click', () => {
  const name = downloadIno();
  if (name) setStatus(`backed up ${name} to your Downloads`);
});

// ---------- pattern management (duplicate / rename / delete) ----------
// These write/remove files, so they go through serve.py. id-ify free text:
// lowercase, non [a-z0-9_-] -> '_', trim underscores.
function idify(s) {
  if (!s) return null;
  const v = s.trim().toLowerCase().replace(/[^a-z0-9_-]+/g, '_').replace(/^_+|_+$/g, '');
  return v || null;
}

async function serverOp(endpoint, body) {
  const res = await fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  if (!res.ok) {
    const j = await res.json().catch(() => ({}));
    throw new Error(j.error || `HTTP ${res.status} (is serve.py running?)`);
  }
  return res.json().catch(() => ({}));
}

duplicateBtn.addEventListener('click', async () => {
  if (!current) return;
  const to = idify(prompt(`Duplicate "${current.id}" as:`, `${current.id}_v2`));
  if (!to) return;
  if (patternIds.includes(to)) { setStatus(`"${to}" already exists`); return; }
  try {
    await serverOp('duplicate', { from: current.id, to, content: outputSource() });
    patternIds.push(to);
    buildList();
    setStatus(`duplicated ${current.id} → ${to} (current tuning baked in)`);
    selectPattern(to);
  } catch (e) { setStatus(`duplicate failed: ${e.message}`); }
});

renameBtn.addEventListener('click', async () => {
  if (!current) return;
  const from = current.id;
  const to = idify(prompt(`Rename "${from}" to:`, from));
  if (!to || to === from) return;
  if (patternIds.includes(to)) { setStatus(`"${to}" already exists`); return; }
  try {
    await serverOp('rename', { from, to, content: outputSource() });
    reviews.renameReview(from, to);
    if (tuning[from]) { tuning[to] = tuning[from]; delete tuning[from]; saveTuning(); }
    patternIds[patternIds.indexOf(from)] = to;
    buildList();
    setStatus(`renamed ${from} → ${to}`);
    selectPattern(to);
  } catch (e) { setStatus(`rename failed: ${e.message}`); }
});

deleteBtn.addEventListener('click', async () => {
  if (!current) return;
  const id = current.id;
  const builtin = PATTERNS.includes(id);
  const msg = builtin
    ? `Delete built-in "${id}"? The patterns/ source is removed (recoverable via git).`
    : `Delete variant "${id}"? This removes its .ino for good.`;
  if (!confirm(msg)) return;
  try {
    await serverOp('delete', { id });
    reviews.deleteReview(id);
    if (tuning[id]) { delete tuning[id]; saveTuning(); }
    const idx = patternIds.indexOf(id);
    patternIds = patternIds.filter((x) => x !== id);
    buildList();
    setStatus(`deleted ${id}`);
    if (patternIds.length) {
      selectPattern(patternIds[Math.min(idx, patternIds.length - 1)]);
    } else {
      current = null;
      descEl.classList.add('empty');
      descEl.textContent = 'No patterns. Duplicate one to start.';
      reviewEl.innerHTML = '';
      knobsEl.innerHTML = '';
    }
  } catch (e) { setStatus(`delete failed: ${e.message}`); }
});

// ---------- serial ----------
const serial = { writer: null, active: false, pending: null };
let lastSend = 0;
const encoder = new TextEncoder();
connectBtn.addEventListener('click', async () => {
  if (!('serial' in navigator)) { setStatus('Web Serial needs Chrome/Edge on localhost or https'); return; }
  try {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    serial.writer = port.writable.getWriter();
    serial.active = true;
    connectBtn.classList.add('active');
    connectBtn.textContent = '✓ Streaming serial';
    setStatus('serial open — streaming frames at 30 fps');
  } catch (e) { setStatus('serial error: ' + e.message); }
});
function maybeSend(buf) {
  if (!serial.active || !serial.writer || serial.pending) return;
  const now = performance.now();
  if (now - lastSend < 33) return;
  lastSend = now;
  let s = '';
  for (let i = 0; i < buf.length; i++) s += buf[i] + (i < buf.length - 1 ? ',' : '\n');
  serial.pending = serial.writer.write(encoder.encode(s))
    .catch((e) => setStatus('serial write failed: ' + e.message))
    .finally(() => (serial.pending = null));
}

// ---------- global brightness ramp (mirrors SozoCumulative.ino dimGain) ----------
// Per-LED gain: 1.0 at the start of each line, ramping down to `floor` by the
// line's 60% point, then held. Counted in LEDs from the start of each line
// (line 1 from idx 0, line 2 from idx 15). Preview-only; tune here, copy the
// readout into SozoCumulative.ino.
const DIM_KEY = 'sozo_studio_dim';
const dim = { enabled: true, floor: 0.60, topPoint: 8, botPoint: 7, topRamp: 6, botRamp: 6 };
(function loadDim() {
  try { Object.assign(dim, JSON.parse(localStorage.getItem(DIM_KEY)) || {}); } catch { /* ignore */ }
})();
function saveDim() { try { localStorage.setItem(DIM_KEY, JSON.stringify(dim)); } catch { /* ignore */ } }

function dimGain(i) {
  let pos, point, ramp;
  if (i < 15) { pos = i;       point = dim.topPoint; ramp = dim.topRamp; }
  else        { pos = i - 15;  point = dim.botPoint; ramp = dim.botRamp; }
  if (ramp <= 0) return pos < point ? 1 : dim.floor;
  let frac = (pos - (point - ramp)) / ramp;       // 0 at ramp start, 1 at the point
  frac = Math.max(0, Math.min(1, frac));
  return 1 - frac * (1 - dim.floor);              // 1.0 -> floor
}
function applyDimMask(buf) {
  if (!dim.enabled) return;
  for (let i = 0; i < buf.length; i++) buf[i] = Math.round(buf[i] * dimGain(i));
}

let dimReadoutEl = null;
function refreshDimReadout() {
  if (!dimReadoutEl) return;
  const f = (v) => (Number.isInteger(v) ? String(v) : (+v).toPrecision(3).replace(/\.?0+$/, ''));
  dimReadoutEl.textContent =
    `// paste into SozoCumulative.ino\n` +
    `DIM_FLOOR     = ${dim.floor.toFixed(2)}f;\n` +
    `TOP_60_POINT  = ${f(dim.topPoint)}f;   TOP_RAMP_DIST = ${f(dim.topRamp)}f;\n` +
    `BOT_60_POINT  = ${f(dim.botPoint)}f;   BOT_RAMP_DIST = ${f(dim.botRamp)}f;`;
}

function makeDimKnob(label, key, min, max, step) {
  const wrap = document.createElement('div');
  wrap.className = 'knob';
  const lab = document.createElement('div');
  lab.className = 'knob-label';
  const nm = document.createElement('span');
  nm.className = 'name';
  nm.textContent = label;
  lab.appendChild(nm);
  wrap.appendChild(lab);
  const row = document.createElement('div');
  row.className = 'knob-row';
  const range = document.createElement('input');
  range.type = 'range'; range.min = min; range.max = max; range.step = step; range.value = dim[key];
  const num = document.createElement('input');
  num.type = 'number'; num.step = step; num.value = dim[key];
  const apply = (v) => { dim[key] = v; range.value = v; num.value = round(v); saveDim(); refreshDimReadout(); };
  range.addEventListener('input', () => apply(parseFloat(range.value)));
  num.addEventListener('change', () => { const v = parseFloat(num.value); if (!Number.isNaN(v)) apply(v); });
  row.append(range, num);
  wrap.appendChild(row);
  return wrap;
}

function buildDimControls() {
  dimEl.innerHTML = '';
  const h = document.createElement('h2');
  h.textContent = 'Global Brightness Ramp';
  dimEl.appendChild(h);

  const row = document.createElement('label');
  row.className = 'toggle-row';
  const cb = document.createElement('input');
  cb.type = 'checkbox';
  cb.checked = dim.enabled;
  const txt = document.createElement('span');
  txt.textContent = 'Apply ramp to preview';
  cb.addEventListener('change', () => { dim.enabled = cb.checked; saveDim(); });
  row.append(cb, txt);
  dimEl.appendChild(row);

  dimEl.appendChild(makeDimKnob('Floor level (0–1)', 'floor', 0, 1, 0.01));
  dimEl.appendChild(makeDimKnob('Top line 60% point (LEDs)', 'topPoint', 0, 15, 0.5));
  dimEl.appendChild(makeDimKnob('Top ramp distance (LEDs)', 'topRamp', 0.5, 15, 0.5));
  dimEl.appendChild(makeDimKnob('Bottom line 60% point (LEDs)', 'botPoint', 0, 15, 0.5));
  dimEl.appendChild(makeDimKnob('Bottom ramp distance (LEDs)', 'botRamp', 0.5, 15, 0.5));

  dimReadoutEl = document.createElement('pre');
  dimReadoutEl.className = 'dim-readout';
  dimEl.appendChild(dimReadoutEl);
  refreshDimReadout();
}
buildDimControls();

// ---------- gallery player ----------
// Plays the whole SozoBasic gallery in order, mirroring the firmware's gallery
// stage: each pattern fades in, holds, fades out, then the next begins; loops
// forever. Runs from its own compiled clips (with your saved tuning baked in),
// independent of the currently-selected pattern. (State object is defined up top,
// next to GALLERY_IDS, because buildList() reads it at load time.)
function saveGalleryCfg() {
  try { localStorage.setItem(GAL_KEY, JSON.stringify({ holdSec: gallery.holdSec, fadeSec: gallery.fadeSec })); } catch { /* ignore */ }
}

// Compile one pattern into a self-contained clip, applying saved tuning + any
// reversal — the same resolution selectPattern() does, but without touching the
// DOM or the current selection.
async function buildClip(id) {
  const url = new URL(`../patterns/${id}/${id}.ino`, import.meta.url);
  const source = await (await fetch(url)).text();
  let compiled = transpile(source);
  const params = {};
  for (const c of compiled.consts) if (c.editable) params[c.name] = c.number;
  const cached = tuning[id];
  if (cached?.params) for (const k in cached.params) if (k in params) params[k] = cached.params[k];
  let useParams = params;
  const spec = DIRECTIONAL[id] || null;
  if (cached?.reversed && spec) {
    if (spec.source) { const cr = transpile(spec.source(source)); if (cr.ok) compiled = cr; }
    else if (spec.negate) { useParams = { ...params }; for (const n of spec.negate) if (n in useParams) useParams[n] = -useParams[n]; }
  }
  const state = compiled.ok ? compiled.makeState(useParams) : null;
  return { id, compiled, params: useParams, state };
}

function toggleGallery() { gallery.active ? stopGallery() : startGallery(); }

async function startGallery() {
  const ids = GALLERY_IDS.filter((id) => patternIds.includes(id));
  if (!ids.length) { setStatus('no gallery patterns to play'); return; }
  setStatus('loading gallery…');
  try {
    const clips = [];
    for (const id of ids) {
      const clip = await buildClip(id);
      if (clip.compiled.ok) clips.push(clip);   // skip any that can't preview
    }
    if (!clips.length) { setStatus('no playable gallery patterns'); return; }
    gallery.clips = clips;
    gallery.idx = 0;
    gallery.clipStart = performance.now();
    gallery.lastNow = gallery.clipStart;
    gallery.active = true;
    highlightGallery();
  } catch (e) {
    setStatus('gallery load failed: ' + e.message);
  }
}

function stopGallery() {
  gallery.active = false;
  highlightGallery();
  if (current) resetState();   // hand the preview back to the selected pattern
  setStatus('gallery stopped');
}

// Reflect play state in the ▶/⏹ button and highlight the playing member row.
function highlightGallery() {
  const activeId = gallery.active && gallery.clips[gallery.idx] ? gallery.clips[gallery.idx].id : null;
  for (const [id, ref] of liById) ref.li.classList.toggle('playing', id === activeId);
  const btn = document.getElementById('galleryPlay');
  if (btn) btn.textContent = gallery.active ? '⏹' : '▶';
}

function tickGallery() {
  const now = performance.now();
  const dt = Math.min((now - gallery.lastNow) / 1000, 0.1);
  gallery.lastNow = now;

  const fade = gallery.fadeSec;
  const hold = gallery.holdSec;
  const total = 2 * fade + hold;          // fade in + hold + fade out
  let elapsed = (now - gallery.clipStart) / 1000;

  if (elapsed >= total) {                 // advance to the next pattern, looping
    gallery.idx = (gallery.idx + 1) % gallery.clips.length;
    gallery.clipStart = now;
    elapsed = 0;
    const nc = gallery.clips[gallery.idx];
    if (nc.compiled.ok) nc.state = nc.compiled.makeState(nc.params);
    highlightGallery();
  }

  const clip = gallery.clips[gallery.idx];
  let env = 1;                            // brightness envelope 0..1
  if (fade > 0) {
    if (elapsed < fade) env = elapsed / fade;
    else if (elapsed > fade + hold) env = Math.max(0, 1 - (elapsed - fade - hold) / fade);
  }

  out.fill(0);
  try { clip.compiled.step(elapsed, dt, clip.params, clip.state, out); }
  catch (err) { setStatus('gallery render error: ' + err.message); }
  if (env < 1) for (let i = 0; i < out.length; i++) out[i] = Math.round(out[i] * env);
  applyDimMask(out);
  for (let i = 0; i < out.length; i++) setLed(i, out[i]);
  maybeSend(out);
  setStatus(`▶ gallery ${gallery.idx + 1}/${gallery.clips.length} — ${clip.id}`);
}

// ---------- render loop ----------
function tick() {
  requestAnimationFrame(tick);
  if (gallery.active) { tickGallery(); return; }
  const t = (performance.now() - clockStart) / 1000;
  const dt = lastT === 0 ? 1 / 60 : Math.min(t - lastT, 0.1);
  lastT = t;

  const e = current ? effective() : null;
  if (e?.compiled?.ok) {
    out.fill(0);
    try {
      e.compiled.step(t, dt, e.params, state, out);
    } catch (err) {
      setStatus('render error: ' + err.message);
    }
    applyDimMask(out);
    for (let i = 0; i < out.length; i++) setLed(i, out[i]);
    maybeSend(out);
  } else {
    const phase = (t * 0.7) % 1;
    setAll(phase < 0.5 ? 160 : 0);
  }
}
tick();

// Pull the live pattern list (built-ins + variants) from serve.py, then open the
// first one. Falls back to the static manifest if the server isn't running.
loadPatternList().then(() => {
  if (patternIds.length) selectPattern(patternIds[0]);
});
