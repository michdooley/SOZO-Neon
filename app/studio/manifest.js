// Static fallback list of patterns the Studio can open, used only when the
// Studio is opened over file:// or a plain `http.server` (no serve.py).
//
// When you run `python3 studio/serve.py`, the Studio ignores this list and
// scans app/patterns/ live instead — so new patterns show up automatically.
// This repo ships with patterns/ empty (a clean slate); reference sketches
// live in app/examples/. Copy one into app/patterns/<id>/<id>.ino to load it.
export const PATTERNS = [];

export const PRETTY = (id) =>
  id.replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
