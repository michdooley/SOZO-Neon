#!/usr/bin/env python3
"""Static server for the SOZO Pattern Studio + a /save endpoint.

Serves the Visualizer/ directory (so studio.html and friends work) and adds
POST /save, which writes an exported sketch straight into refined-patterns/
(created if missing, overwrite on) — so you never touch Downloads.

Run it instead of `python3 -m http.server`:

    python3 studio/serve.py            # serves on http://localhost:8000
    python3 studio/serve.py 8080       # custom port

The plain `python3 -m http.server` still works too; the Studio just falls
back to a normal download when /save isn't available.
"""
import json
import re
import shutil
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

# Serve the Visualizer/ dir (the parent of this script's studio/ folder).
ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "refined-patterns"
PATTERNS_DIR = ROOT / "patterns"
REVIEWS_FILE = ROOT / "studio" / "reviews.json"
SAFE_NAME = re.compile(r"^[A-Za-z0-9_\-]+\.ino$")
SAFE_ID = re.compile(r"^[A-Za-z0-9_\-]+$")
# patterns/ dirs that aren't generators (don't show in the Studio list).
EXCLUDE_IDS = {"dumb_player"}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def _json(self, code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return json.loads(self.rfile.read(length) or b"{}")

    def do_GET(self):
        # GET /patterns -> live list of pattern ids (built-ins + studio variants),
        # discovered by scanning patterns/<id>/<id>.ino. Everything else is static.
        if self.path.rstrip("/") == "/patterns":
            ids = []
            if PATTERNS_DIR.is_dir():
                for d in sorted(PATTERNS_DIR.iterdir()):
                    if d.name in EXCLUDE_IDS:
                        continue
                    if d.is_dir() and (d / f"{d.name}.ino").is_file():
                        ids.append(d.name)
            self._json(200, {"patterns": ids})
            return
        super().do_GET()

    def do_POST(self):
        path = self.path.rstrip("/")
        if path == "/save":
            self._save_ino()
        elif path == "/reviews":
            self._save_reviews()
        elif path == "/duplicate":
            self._dup_or_rename(rename=False)
        elif path == "/rename":
            self._dup_or_rename(rename=True)
        elif path == "/delete":
            self._delete_pattern()
        else:
            self._json(404, {"error": "unknown endpoint"})

    def _dup_or_rename(self, rename):
        # Body: {from, to, content}. Writes patterns/<to>/<to>.ino from `content`.
        # On rename, also removes patterns/<from>/ and migrates any refined export.
        try:
            data = self._read_json_body()
            frm = str(data.get("from", ""))
            to = str(data.get("to", ""))
            content = data.get("content", "")
            if not SAFE_ID.match(to):
                self._json(400, {"error": f"unsafe name: {to!r}"})
                return
            dest_dir = PATTERNS_DIR / to
            if dest_dir.exists() and to != frm:
                self._json(400, {"error": f'"{to}" already exists'})
                return
            if rename:
                if not SAFE_ID.match(frm) or not (PATTERNS_DIR / frm).is_dir():
                    self._json(404, {"error": f"no such pattern: {frm!r}"})
                    return
            dest_dir.mkdir(parents=True, exist_ok=True)
            (dest_dir / f"{to}.ino").write_text(content, encoding="utf-8")
            if rename and to != frm:
                shutil.rmtree(PATTERNS_DIR / frm, ignore_errors=True)
                ref_from = OUT_DIR / f"{frm}.ino"
                if ref_from.exists():
                    ref_from.replace(OUT_DIR / f"{to}.ino")
            self.log_message("%s -> %s", "renamed" if rename else "duplicated", to)
            self._json(200, {"ok": True, "id": to})
        except Exception as e:  # noqa: BLE001
            self._json(500, {"error": str(e)})

    def _delete_pattern(self):
        # Body: {id}. Removes patterns/<id>/ and any refined-patterns/<id>.ino.
        try:
            data = self._read_json_body()
            pid = str(data.get("id", ""))
            if not SAFE_ID.match(pid):
                self._json(400, {"error": f"unsafe id: {pid!r}"})
                return
            d = PATTERNS_DIR / pid
            if d.is_dir():
                shutil.rmtree(d, ignore_errors=True)
            ref = OUT_DIR / f"{pid}.ino"
            if ref.exists():
                ref.unlink()
            self.log_message("deleted %s", pid)
            self._json(200, {"ok": True})
        except Exception as e:  # noqa: BLE001
            self._json(500, {"error": str(e)})

    def _save_ino(self):
        try:
            length = int(self.headers.get("Content-Length", 0))
            data = json.loads(self.rfile.read(length) or b"{}")
            name = str(data.get("name", ""))
            content = data.get("content", "")
            if not SAFE_NAME.match(name):
                self._json(400, {"error": f"unsafe filename: {name!r}"})
                return
            OUT_DIR.mkdir(parents=True, exist_ok=True)
            dest = OUT_DIR / name  # overwrite
            dest.write_text(content, encoding="utf-8")
            rel = dest.relative_to(ROOT).as_posix()
            self.log_message("wrote %s (%d bytes)", rel, len(content))
            self._json(200, {"ok": True, "path": rel})
        except Exception as e:  # noqa: BLE001
            self._json(500, {"error": str(e)})

    def _save_reviews(self):
        # Persist the studio's per-pattern reviews blob to studio/reviews.json.
        try:
            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length) or b"{}"
            parsed = json.loads(raw)  # validate it's JSON before writing
            REVIEWS_FILE.parent.mkdir(parents=True, exist_ok=True)
            REVIEWS_FILE.write_text(
                json.dumps(parsed, indent=2, sort_keys=True), encoding="utf-8"
            )
            self.log_message("wrote %s (%d patterns)", REVIEWS_FILE.name, len(parsed))
            self._json(200, {"ok": True})
        except Exception as e:  # noqa: BLE001
            self._json(500, {"error": str(e)})


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"SOZO Pattern Studio")
    print(f"  serving {ROOT}")
    print(f"  saving  -> {OUT_DIR}")
    print(f"  open    http://localhost:{port}/studio.html")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye")


if __name__ == "__main__":
    main()
