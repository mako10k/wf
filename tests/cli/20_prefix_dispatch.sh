#!/bin/sh
# Prefix dispatch for top-level commands (unique prefixes should reach handlers)

set -e
WF=${WF:-../../src/wf}

# Use timeout if available so that make test never hangs on an unexpected
# interactive prompt (e.g. future changes that hit passwd/login for "user").
if command -v timeout >/dev/null 2>&1; then
  RUN="timeout 3"
else
  RUN=""
fi

# These will fail later (no domain / auth / tty) but the important thing is
# that we do NOT get "unknown command". We look for handler-specific messages.

echo "=== 'logi assistant' should reach login handler (assistant path has no password prompt) ==="
out=$($RUN "$WF" logi assistant 2>&1 || true)
case "$out" in
  *"unknown command"*) echo "FAIL: 'logi' was treated as unknown" >&2; exit 1 ;;
  *"export WF_TOKEN"*|*"usage:"*) echo "  (reached login path, good)" ;;
  *) echo "  (got: ${out%%$'\n'*})" ;;
esac

echo "=== 'logo' should reach logout ==="
out=$($RUN "$WF" logo 2>&1 || true)
case "$out" in
  *"unknown command"*) echo "FAIL: 'logo' was treated as unknown" >&2; exit 1 ;;
  *"unset WF_TOKEN"*|*"usage:"*) echo "  (reached logout path, good)" ;;
  *) echo "  (got: ${out%%$'\n'*})" ;;
esac

echo "=== 'pas assistant Assistant' should reach passwd handler (assistant skips password read) ==="
out=$($RUN "$WF" pas assistant Assistant 2>&1 || true)
case "$out" in
  *"unknown command"*) echo "FAIL: 'pas' was treated as unknown" >&2; exit 1 ;;
  *"assistant registered"*|*"usage:"*) echo "  (reached passwd path, good)" ;;
  *) echo "  (got: ${out%%$'\n'*})" ;;
esac

echo "20_prefix_dispatch.sh: OK"
