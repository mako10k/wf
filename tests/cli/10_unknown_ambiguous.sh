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

echo "=== ambiguous top-level (e matches exec+env) ==="
if "$WF" e 2>&1 | grep -q "ambiguous command: e"; then
  :
else
  echo "expected 'ambiguous command: e'" >&2
  exit 1
fi

echo "=== env prefix is unique once extended to 'en' ==="
if "$WF" en help 2>&1 | grep -q "usage: wf env COMMAND"; then
  :
else
  echo "expected 'en' to dispatch to env" >&2
  exit 1
fi

echo "10_unknown_ambiguous.sh: OK"
