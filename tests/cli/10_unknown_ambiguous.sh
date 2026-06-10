#!/bin/sh
# Unknown command and ambiguous prefix handling at top level

set -e
WF=${WF:-../../src/wf}

echo "=== unknown command ==="
if "$WF" foobarbaz 2>&1 | grep -q "unknown command: foobarbaz"; then
  :
else
  echo "expected 'unknown command' message" >&2
  exit 1
fi

echo "=== ambiguous top-level (l matches login+logout) ==="
if "$WF" l 2>&1 | grep -q "ambiguous command: l"; then
  :
else
  echo "expected 'ambiguous command: l'" >&2
  exit 1
fi

echo "=== ambiguous (lo still ambiguous) ==="
"$WF" lo 2>&1 | grep -q "ambiguous command: lo"

echo "10_unknown_ambiguous.sh: OK"
