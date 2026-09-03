// SOZO Pattern Studio — per-pattern reviews (rating / flag / notes) + filtering.
//
// Data shape:  reviews[id] = { rating: 0..5, flag: 'none'|'accept'|'reject', notes: '' }
//
// Persistence: localStorage is the live store. On every change we also best-effort
// POST the whole blob to serve.py (-> studio/reviews.json) so the reviews are a
// committable artifact. On first load, if localStorage is empty, we seed from that
// file. With plain http.server / file://, the POST just no-ops and localStorage
// carries everything.

const KEY = 'sozo_studio_reviews';
export const FLAG_GLYPH = { none: '', accept: '✓', reject: '✕' };

let data = {};
const filter = { flags: new Set(), minRating: 0 };
let saveTimer = null;

// ---------- store ----------
export function loadReviews() {
  try { data = JSON.parse(localStorage.getItem(KEY)) || {}; } catch { data = {}; }
  return data;
}

export async function seedFromServer(url) {
  if (Object.keys(data).length) return false;      // localStorage wins if non-empty
  try {
    const r = await fetch(url);
    if (!r.ok) return false;
    const j = await r.json();
    if (j && typeof j === 'object') { data = j; persistLocal(); return true; }
  } catch { /* no server / no file */ }
  return false;
}

export function getReview(id) {
  return data[id] || { rating: 0, flag: 'none', notes: '' };
}

export function setReview(id, patch) {
  data[id] = { ...getReview(id), ...patch };
  persist();
}

// Move a review to a new id (on pattern rename). No-op if `from` has none.
export function renameReview(from, to) {
  if (data[from] && from !== to) {
    data[to] = data[from];
    delete data[from];
    persist();
  }
}

// Drop a review (on pattern delete).
export function deleteReview(id) {
  if (data[id]) { delete data[id]; persist(); }
}

function persistLocal() {
  try { localStorage.setItem(KEY, JSON.stringify(data)); } catch { /* quota / disabled */ }
}

function persist() {
  persistLocal();
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    fetch('reviews', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data, null, 2),
    }).catch(() => { /* serve.py not running — localStorage already has it */ });
  }, 400);
}

// ---------- filter ----------
export function passesFilter(id) {
  const r = getReview(id);
  if (filter.flags.size && !filter.flags.has(r.flag)) return false;
  if (r.rating < filter.minRating) return false;
  return true;
}

export function renderFilter(container, onChange) {
  container.innerHTML = '';

  const flagRow = document.createElement('div');
  flagRow.className = 'filt-row';
  const flab = document.createElement('span');
  flab.className = 'filt-label';
  flab.textContent = 'Flag';
  flagRow.appendChild(flab);
  for (const [val, glyph] of [['accept', '✓'], ['reject', '✕'], ['none', '—']]) {
    const c = document.createElement('button');
    c.className = `chip ${val}`;
    c.textContent = glyph;
    c.title = `show ${val}`;
    c.classList.toggle('on', filter.flags.has(val));
    c.addEventListener('click', () => {
      if (filter.flags.has(val)) filter.flags.delete(val); else filter.flags.add(val);
      c.classList.toggle('on');
      onChange?.();
    });
    flagRow.appendChild(c);
  }
  container.appendChild(flagRow);

  const rateRow = document.createElement('div');
  rateRow.className = 'filt-row';
  const rlab = document.createElement('span');
  rlab.className = 'filt-label';
  rlab.textContent = 'Min ★';
  rateRow.appendChild(rlab);
  const stars = document.createElement('div');
  stars.className = 'minstars';
  for (let k = 0; k <= 5; k++) {
    const b = document.createElement('button');
    b.className = 'chip';
    b.textContent = k === 0 ? 'any' : String(k);
    b.classList.toggle('on', filter.minRating === k);
    b.addEventListener('click', () => {
      filter.minRating = k;
      for (const x of stars.children) x.classList.remove('on');
      b.classList.add('on');
      onChange?.();
    });
    stars.appendChild(b);
  }
  rateRow.appendChild(stars);
  container.appendChild(rateRow);
}

// ---------- per-pattern editor ----------
export function renderEditor(container, id, onChange) {
  container.innerHTML = '';
  if (!id) return;
  const r = getReview(id);

  const h = document.createElement('h2');
  h.textContent = 'Review';
  container.appendChild(h);

  // rating stars (click current value to clear)
  const stars = document.createElement('div');
  stars.className = 'stars';
  for (let k = 1; k <= 5; k++) {
    const s = document.createElement('span');
    s.className = 'star';
    s.textContent = '★';
    s.classList.toggle('on', k <= r.rating);
    s.addEventListener('click', () => {
      const cur = getReview(id).rating;
      setReview(id, { rating: cur === k ? 0 : k });
      renderEditor(container, id, onChange);
      onChange?.();
    });
    stars.appendChild(s);
  }
  container.appendChild(stars);

  // flag buttons
  const fb = document.createElement('div');
  fb.className = 'flagbtns';
  for (const [val, glyph, title] of [['none', '—', 'no flag'], ['accept', '✓', 'accept'], ['reject', '✕', 'reject']]) {
    const b = document.createElement('button');
    b.className = `flagbtn ${val}`;
    b.textContent = glyph;
    b.title = title;
    b.classList.toggle('active', r.flag === val);
    b.addEventListener('click', () => {
      setReview(id, { flag: val });
      renderEditor(container, id, onChange);
      onChange?.();
    });
    fb.appendChild(b);
  }
  container.appendChild(fb);

  // notes (no re-render on input, so the field keeps focus)
  const ta = document.createElement('textarea');
  ta.className = 'notes';
  ta.placeholder = 'Notes — what to improve / change…';
  ta.value = r.notes || '';
  ta.addEventListener('input', () => {
    setReview(id, { notes: ta.value });
    onChange?.();
  });
  container.appendChild(ta);
}
