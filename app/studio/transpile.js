// SOZO pattern studio — house-dialect Arduino transpiler.
//
// The .ino files are the single source of truth. This module reads one
// pattern sketch and produces:
//   - the tunable top-of-file `const` scalars (slider candidates),
//   - the const arrays (XS/YS/PHASES/...),
//   - any stateful globals + structs (for the 6 stateful patterns),
//   - a JS `step(t, dt, P, S, out)` function transpiled from `loop()`,
//   - a serialize(params) that re-emits the .ino with edited constants.
//
// It is NOT a general C++ compiler. It targets exactly the idioms every
// SOZO pattern shares: a const block, a `loop()` over NUM_LEDS using
// fmod/sin/cos/exp/sqrt/fabs/pow/PI/millis/random, and a Serial.print
// tail. Anything outside that vocabulary is reported as a warning and
// the pattern falls back to "edit + export works, preview unavailable".

// ---------- small helpers ----------

function stripComments(src) {
  // Remove block + line comments. No string literals contain comment
  // markers in this corpus (only ',' and '\n' separators), so this is safe.
  return src
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/\/\/[^\n]*/g, '');
}

// A number literal, possibly with an f/F/L/UL suffix, e.g. 8.0f, 255, 1103515245UL.
const NUM_RE = /^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFlLuU]*$/;

function isPureNumber(expr) {
  return NUM_RE.test(expr.trim());
}

function parseNumber(tok) {
  return parseFloat(String(tok).replace(/[fFlLuU]+$/, ''));
}

// Convert a house-dialect C expression/statement chunk to JS by stripping
// type suffixes and casts. Used both for derived consts and the loop body.
function cExprToJs(s) {
  let out = s;
  // numeric suffixes: 1.0f -> 1.0, 12345UL -> 12345, 5L -> 5
  out = out.replace(/\b(\d+\.?\d*(?:[eE][+-]?\d+)?)[fF]\b/g, '$1');
  out = out.replace(/\b(\d+)(?:UL|LU|ul|lu|U|u|L|l)\b/g, '$1');
  // casts -> truncation / no-op. order: longest first.
  out = out.replace(/\(\s*unsigned\s+long\s*\)/g, 'Math.trunc');
  out = out.replace(/\(\s*long\s*\)/g, 'Math.trunc');
  out = out.replace(/\(\s*int\s*\)/g, 'Math.trunc');
  out = out.replace(/\(\s*uint8_t\s*\)/g, 'Math.trunc');
  out = out.replace(/\(\s*float\s*\)/g, '');
  return out;
}

// ---------- section splitting ----------

function findBlock(src, header) {
  // Returns {body, start, end} for `header ... { ... }` with brace matching.
  const hIdx = src.indexOf(header);
  if (hIdx < 0) return null;
  const open = src.indexOf('{', hIdx);
  if (open < 0) return null;
  let depth = 0;
  for (let i = open; i < src.length; i++) {
    if (src[i] === '{') depth++;
    else if (src[i] === '}') {
      depth--;
      if (depth === 0) return { body: src.slice(open + 1, i), start: hIdx, end: i + 1 };
    }
  }
  return null;
}

// ---------- declaration parsing ----------

function parseConsts(declRegion) {
  // scalar consts: const <int|float> NAME = <expr> ;   (no '[' before '=')
  const scalars = [];
  const scalarRe = /const\s+(int|float)\s+([A-Za-z_]\w*)\s*=\s*([^;{]+);/g;
  let m;
  while ((m = scalarRe.exec(declRegion))) {
    const [, type, name, rawValue] = m;
    const value = rawValue.trim();
    scalars.push({
      type, name, raw: value,
      editable: isPureNumber(value),
      number: isPureNumber(value) ? parseNumber(value) : null,
    });
  }
  return scalars;
}

function parseArrays(declRegion) {
  const arrays = [];
  const arrRe = /const\s+(int|float)\s+([A-Za-z_]\w*)\s*\[\s*([A-Za-z_0-9]*)\s*\]\s*=\s*\{([\s\S]*?)\}\s*;/g;
  let m;
  while ((m = arrRe.exec(declRegion))) {
    const [, type, name, size, body] = m;
    const nums = body
      .split(',')
      .map((s) => s.trim())
      .filter((s) => s.length)
      .map(parseNumber);
    arrays.push({ type, name, size, values: nums });
  }
  return arrays;
}

function parseStructs(declRegion) {
  const structs = {};
  const sRe = /struct\s+([A-Za-z_]\w*)\s*\{([\s\S]*?)\}\s*;/g;
  let m;
  while ((m = sRe.exec(declRegion))) {
    const [, name, body] = m;
    const fields = [];
    const fRe = /(unsigned\s+long|long|float|int|uint8_t|bool)\s+([A-Za-z_]\w*)\s*;/g;
    let f;
    while ((f = fRe.exec(body))) {
      const [, type, fname] = f;
      fields.push({ type, name: fname, init: type === 'bool' ? 'false' : '0' });
    }
    structs[name] = fields;
  }
  return structs;
}

function parseStateGlobals(declRegion, structs) {
  // Mutable (non-const) globals declared before setup(). Two forms:
  //   StructName arr[SIZE];           -> array of struct objects
  //   <float|int|...> name [= init];  -> scalar carried across frames
  const states = [];
  // strip out the things we've already parsed so they don't re-match
  let region = declRegion
    .replace(/const\s+(int|float)\s+[A-Za-z_]\w*\s*(\[[^\]]*\])?\s*=\s*(\{[\s\S]*?\}|[^;{]+);/g, '')
    .replace(/struct\s+[A-Za-z_]\w*\s*\{[\s\S]*?\}\s*;/g, '')
    .replace(/#include[^\n]*/g, '');

  const structNames = Object.keys(structs);
  if (structNames.length) {
    const arrRe = new RegExp(
      `\\b(${structNames.join('|')})\\s+([A-Za-z_]\\w*)\\s*\\[\\s*([A-Za-z_0-9]*)\\s*\\]\\s*;`,
      'g',
    );
    let m;
    while ((m = arrRe.exec(region))) {
      states.push({ kind: 'structArray', struct: m[1], name: m[2], size: m[3] });
    }
    region = region.replace(arrRe, '');
  }

  const scalarRe = /\b(unsigned\s+long|long|float|int|uint8_t|bool)\s+([A-Za-z_]\w*)\s*(=\s*([^;]+))?;/g;
  let m;
  while ((m = scalarRe.exec(region))) {
    const init = m[4] != null ? cExprToJs(m[4].trim()) : (m[1] === 'bool' ? 'false' : '0');
    states.push({ kind: 'scalar', type: m[1], name: m[2], init });
  }
  return states;
}

// ---------- loop body transpilation ----------

function transpileLoopBody(loopBody, stateNames, knownConsts) {
  let s = loopBody;

  // 1) drop the serial separator prints and delays.
  s = s.replace(/Serial\s*\.\s*print(?:ln)?\s*\([^;]*?(?:\\n|'\\n')[^;]*?\);/g, '');
  s = s.replace(/delay\s*\([^;]*\)\s*;/g, '');
  // 2) the value print -> out[i] = clamp(value)
  s = s.replace(/Serial\s*\.\s*print(?:ln)?\s*\(\s*([^;]*?)\s*\)\s*;/g, 'out[i] = __cb($1);');

  // 3) local fixed-size array decls: uint8_t buf[NUM_LEDS]; -> let buf = ...
  s = s.replace(
    /\b(?:uint8_t|int|float)\s+([A-Za-z_]\w*)\s*\[\s*([A-Za-z_0-9]*)\s*\]\s*;/g,
    'let $1 = new Array($2).fill(0);',
  );

  // 4) numeric suffixes + casts.
  s = cExprToJs(s);

  // 5) declaration type keywords -> let (locals, for-headers).
  // local `const <type> X` keeps const, just drops the C type.
  s = s.replace(/\bconst\s+(?:unsigned\s+long|long|float|int|uint8_t|bool)\s+/g, 'const ');
  s = s.replace(/\bfor\s*\(\s*(?:unsigned\s+long|long|float|int|uint8_t|bool)\s+/g, 'for (let ');
  // statement-leading decls: preceded by ; { } or newline
  s = s.replace(
    /([;{}\)]\s*|\n\s*)(?:unsigned\s+long|long|float|int|uint8_t|bool)\s+([A-Za-z_]\w*)/g,
    '$1let $2',
  );

  // 6) state globals -> S.<name>
  for (const name of stateNames) {
    s = s.replace(new RegExp(`\\b${name}\\b`, 'g'), `S.${name}`);
  }

  return s;
}

function buildStateInit(states, structs, constMap) {
  // Returns (P) => freshStateObject. Sizes resolve from edited params first,
  // then the parsed const defaults, so count knobs actually resize state.
  return function makeState(P) {
    P = P || {};
    const sizeOf = (sz) =>
      (sz in P ? Math.round(P[sz]) : sz in constMap ? constMap[sz] : parseInt(sz, 10) || 0);
    const S = {};
    // deterministic PRNG so preview is stable frame-to-frame.
    S.__r = mulberry32(0x1a2b3c4d);
    for (const st of states) {
      if (st.kind === 'structArray') {
        const n = sizeOf(st.size);
        const fields = structs[st.struct] || [];
        S[st.name] = Array.from({ length: n }, () => {
          const o = {};
          for (const f of fields) o[f.name] = f.init === 'false' ? false : 0;
          return o;
        });
      } else {
        // scalar init may reference consts; evaluate in a tiny scope.
        let v = 0;
        try {
          // eslint-disable-next-line no-new-func
          v = Function(...Object.keys(constMap), `return (${st.init});`)(...Object.values(constMap));
        } catch {
          v = Number(st.init) || 0;
        }
        S[st.name] = v;
      }
    }
    return S;
  };
}

function mulberry32(a) {
  return function (lo, hi) {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    const u = ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    if (lo === undefined) return u;
    if (hi === undefined) return Math.floor(u * lo);
    return lo + Math.floor(u * (hi - lo));
  };
}

// ---------- public API ----------

export function transpile(source) {
  const result = {
    ok: false,
    error: null,
    warnings: [],
    consts: [],
    arrays: [],
    structs: {},
    states: [],
    step: null,
    makeState: null,
    source,
  };

  try {
    const clean = stripComments(source);
    const setupBlk = findBlock(clean, 'void setup');
    const loopBlk = findBlock(clean, 'void loop');
    if (!loopBlk) {
      result.error = 'no loop() found';
      return result;
    }
    const declRegion = clean.slice(0, Math.min(
      setupBlk ? setupBlk.start : clean.length,
      loopBlk.start,
    ));

    const consts = parseConsts(declRegion);
    const arrays = parseArrays(declRegion);
    const structs = parseStructs(declRegion);
    const states = parseStateGlobals(declRegion, structs);

    result.consts = consts;
    result.arrays = arrays;
    result.structs = structs;
    result.states = states;

    // numeric value map for sizes + state-init evaluation (defaults).
    const constMap = {};
    for (const c of consts) {
      if (c.number != null) constMap[c.name] = c.number;
      else {
        try {
          constMap[c.name] = Function(
            ...Object.keys(constMap),
            `return (${cExprToJs(c.raw)});`,
          )(...Object.values(constMap));
        } catch {
          /* derived const we can't pre-eval; injected at runtime instead */
        }
      }
    }

    const stateNames = states.map((s) => s.name);
    const bodyJs = transpileLoopBody(loopBlk.body, stateNames, constMap);

    // Build the const prelude (in declaration order so derived consts resolve).
    const preludeLines = [];
    for (const c of consts) {
      if (c.editable) {
        preludeLines.push(
          `const ${c.name} = (P[${JSON.stringify(c.name)}] !== undefined ? P[${JSON.stringify(c.name)}] : ${c.number});`,
        );
      } else {
        preludeLines.push(`const ${c.name} = (${cExprToJs(c.raw)});`);
      }
    }
    for (const a of arrays) {
      preludeLines.push(`const ${a.name} = __ARR[${JSON.stringify(a.name)}];`);
    }

    const fnSource = `
      "use strict";
      const PI = Math.PI;
      const sin = Math.sin, cos = Math.cos, tan = Math.tan, exp = Math.exp,
            sqrt = Math.sqrt, pow = Math.pow, log = Math.log, atan2 = Math.atan2,
            fabs = Math.abs, abs = Math.abs, floor = Math.floor, ceil = Math.ceil;
      const min = Math.min, max = Math.max;
      const fmod = (x, y) => x % y;
      const millis = () => __t * 1000;
      const random = (lo, hi) => S.__r(lo, hi);
      const __cb = (x) => { x = Math.trunc(x); return x < 0 ? 0 : x > 255 ? 255 : x; };
      ${preludeLines.join('\n      ')}
      ${bodyJs}
    `;

    const __ARR = {};
    for (const a of arrays) __ARR[a.name] = a.values.slice();

    // eslint-disable-next-line no-new-func
    const raw = new Function('__t', '__dt', 'P', 'S', 'out', '__ARR', fnSource);
    result.step = (t, dt, P, S, out) => raw(t, dt, P, S, out, __ARR);
    result.makeState = buildStateInit(states, structs, constMap);
    result.ok = true;
    return result;
  } catch (e) {
    result.error = e.message;
    return result;
  }
}

// Re-emit the .ino with edited scalar-const values. Non-destructive: the
// caller decides where to write. Only pure-number consts are rewritten;
// arrays, structs, and logic are left byte-for-byte intact.
export function serialize(source, paramValues) {
  let out = source;
  for (const [name, value] of Object.entries(paramValues)) {
    // match `const <type> NAME = <number><suffix>;` keeping the type + suffix style.
    const re = new RegExp(
      `(const\\s+(int|float)\\s+${name}\\s*=\\s*)([+-]?[\\d.eE]+)([fF]?)(\\s*;)`,
    );
    out = out.replace(re, (full, pre, type, _num, suffix, post) => {
      let formatted;
      if (type === 'int') formatted = String(Math.round(value));
      else {
        formatted = Number(value).toFixed(4).replace(/\.?0+$/, '');
        if (!formatted.includes('.')) formatted += '.0';
        if (!suffix) suffix = 'f';
      }
      return `${pre}${formatted}${type === 'float' ? (suffix || 'f') : ''}${post}`;
    });
  }
  return out;
}
