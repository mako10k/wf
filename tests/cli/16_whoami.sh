#!/bin/sh
# whoami command behavior.

set -e
WF=${WF:-../../src/wf}

case "$WF" in
  /*) ;;
  *) WF="$(cd "$(dirname "$WF")" && pwd)/$(basename "$WF")" ;;
esac

TESTROOT=$(mktemp -d)
trap 'rm -rf "$TESTROOT"' EXIT

cd "$TESTROOT"
export XDG_DATA_HOME="$TESTROOT/xdg"

"$WF" domain create >/dev/null 2>&1 || true

echo "=== whoami prints the current username ==="
token_out=$("$WF" env export assistant)
TOKEN=$(printf '%s\n' "$token_out" | sed -n "s/^export WF_TOKEN='\([0-9a-f][0-9a-f]*\)'$/\1/p")
[ -n "$TOKEN" ]
out=$(WF_TOKEN="$TOKEN" "$WF" whoami)
[ "$out" = "assistant" ]

echo "=== whoami rejects extra args ==="
out=$(WF_TOKEN="$TOKEN" "$WF" whoami extra 2>&1 || true)
printf '%s\n' "$out" | grep -q '^usage: wf whoami$'

echo "=== whoami fails when not authenticated ==="
out=$("$WF" whoami 2>&1 || true)
printf '%s\n' "$out" | grep -q '^not logged in:'

echo "16_whoami.sh: OK"
