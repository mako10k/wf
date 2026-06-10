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
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== 'logo' should reach logout ==="
out=$($RUN "$WF" logo 2>&1 || true)
case "$out" in
  *"unknown command"*) echo "FAIL: 'logo' was treated as unknown" >&2; exit 1 ;;
  *"unset WF_TOKEN"*|*"usage:"*) echo "  (reached logout path, good)" ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== 'us pas assistant Assistant' should reach user passwd handler (assistant skips password read) ==="
out=$($RUN "$WF" us pas assistant Assistant 2>&1 || true)
case "$out" in
  *"unknown command"*|*"unknown user command"*) echo "FAIL: 'us pas' was treated as unknown" >&2; exit 1 ;;
  *"assistant registered"*|*"usage:"*) echo "  (reached passwd path, good)" ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== 'dom cur' should reach domain handler ==="
out=$($RUN "$WF" dom cur 2>&1 || true)
case "$out" in
  *"unknown command"*) echo "FAIL: 'dom' was treated as unknown" >&2; exit 1 ;;
  *"id:"*|*"path:"*|*"root:"*|*"usage:"*) echo "  (reached domain path, good)" ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== 'dom c' should be ambiguous between create and current ==="
out=$($RUN "$WF" dom c 2>&1 || true)
case "$out" in
  *"ambiguous domain command: c"*) echo "  (ambiguous domain prefix reported, good)" ;;
  *) echo "FAIL: expected ambiguous domain command for 'dom c'" >&2; exit 1 ;;
esac

echo "20_prefix_dispatch.sh: OK"
