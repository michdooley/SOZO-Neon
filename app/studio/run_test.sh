#!/bin/zsh
# Headless transpiler proof using macOS JavaScriptCore (no Node required).
set -e
JSC=/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc
DIR="${0:A:h}"
PAT="$DIR/../patterns"

# Concatenate the (export-stripped) transpiler with the jsc harness.
sed 's/^export //' "$DIR/transpile.js" > /tmp/sozo_transpile_plain.js
cat /tmp/sozo_transpile_plain.js "$DIR/jsc_harness.js" > /tmp/sozo_run.js

# Collect pattern .ino paths (skip js/ and dumb_player).
PATHS=()
for d in "$PAT"/*/; do
  name="${d:t}"
  [[ "$name" == "js" || "$name" == "dumb_player" ]] && continue
  ino="$d$name.ino"
  [[ -f "$ino" ]] && PATHS+=("$ino")
done

"$JSC" /tmp/sozo_run.js -- "${PATHS[@]}"
