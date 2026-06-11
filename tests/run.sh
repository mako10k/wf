#!/bin/sh
# Simple test runner for wf.
# Usage:
#   WF=../src/wf ./run.sh
#   or from project root after build: tests/run.sh

set -e

cd "$(dirname "$0")"

export WF=${WF:-../src/wf}

case "$WF" in
  /*) ;;
  *) WF="$(cd "$(dirname "$WF")" && pwd)/$(basename "$WF")" ;;
esac
export WF
export LANG=C
export LC_ALL=C
export LANGUAGE=C

if [ ! -x "$WF" ]; then
  echo "wf binary not found or not executable at: $WF" >&2
  echo "Build first (make -C src) or set WF=/path/to/wf" >&2
  echo "The test suite also expects a C compiler and libcrypto development headers/libraries." >&2
  exit 1
fi

echo "Using wf: $WF"

# Make the test environment as non-interactive as possible so that
# "make test" / "make check" never hangs waiting for password/tty input
# even if a test accidentally hits an interactive code path.
export TERM=dumb
STTY_STATE=$(stty -g 2>/dev/null || true)
trap '[ -n "$STTY_STATE" ] && stty "$STTY_STATE" 2>/dev/null || true' EXIT
# Best effort: turn off local echo if we have a controlling tty.
stty -echo 2>/dev/null || true

echo


fail=0

run_script() {
  script="$1"
  echo ">>> $script"
  if ( cd "$(dirname "$script")" && sh "$(basename "$script")" ); then
    echo "<<< $script: PASS"
  else
    echo "<<< $script: FAIL" >&2
    fail=1
  fi
  echo
}

# Unit tests (compile & run C tests)
echo "=== Unit tests ==="
mkdir -p tmp
if cc -std=c11 -Wall -Wextra -I.. -I../src -o tmp/test_match \
     unit/test_match.c ../src/util.c -lcrypto 2>&1; then
  if ./tmp/test_match; then
    echo "unit/test_match: PASS"
  else
    echo "unit/test_match: FAIL" >&2
    fail=1
  fi
else
  echo "Failed to compile unit/test_match.c" >&2
  fail=1
fi
echo

# CLI tests
echo "=== CLI / Integration tests ==="
for f in cli/*.sh; do
  [ -f "$f" ] || continue
  run_script "$f" || true
done

if [ $fail -eq 0 ]; then
  echo "All tests passed."
  exit 0
else
  echo "Some tests failed." >&2
  exit 1
fi
