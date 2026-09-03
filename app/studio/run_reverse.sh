#!/bin/zsh
# Prove every directional reverse stays valid and flips direction (JavaScriptCore).
JSC=/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc
DIR="${0:A:h}"
sed 's/^export //' "$DIR/transpile.js"   > /tmp/sozo_t.js
sed 's/^export //' "$DIR/directional.js" > /tmp/sozo_d.js
cat /tmp/sozo_t.js /tmp/sozo_d.js "$DIR/jsc_reverse.js" > /tmp/sozo_runr.js
"$JSC" /tmp/sozo_runr.js -- "$(cd "$DIR/../patterns" && pwd)"
