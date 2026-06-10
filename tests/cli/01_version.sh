#!/bin/sh
# Version command / option tests

set -e
WF=${WF:-../../src/wf}

echo "=== version subcommand prints package version ==="
"$WF" version 2>&1 | grep -q "^wf 0\.1\.0$"

echo "=== --version option prints package version ==="
"$WF" --version 2>&1 | grep -q "^wf 0\.1\.0$"

echo "=== version subcommand rejects extra args ==="
out=$("$WF" version extra 2>&1 || true)
printf '%s\n' "$out" | grep -q "^usage: wf version$"

echo "01_version.sh: OK"