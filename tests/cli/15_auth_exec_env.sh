#!/bin/sh
# Auth command behavior tests for exec/env.

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

DOMAIN_ROOT=$(find "$XDG_DATA_HOME" -type d -path '*/wf/domains/*' | head -1)
if [ -z "$DOMAIN_ROOT" ]; then
  echo "Could not locate domain root under $XDG_DATA_HOME" >&2
  exit 1
fi

echo "=== env export prints shell fragment and records a session ==="
out=$("$WF" env export assistant)
case "$out" in
  export\ WF_TOKEN=\'*\') ;;
  *) echo "FAIL: expected export WF_TOKEN=... output" >&2; exit 1 ;;
esac
token=$(printf '%s\n' "$out" | sed -n "s/^export WF_TOKEN='\([0-9a-f][0-9a-f]*\)'$/\1/p")
if [ -z "$token" ]; then
  echo "FAIL: expected a hex WF_TOKEN in env export output" >&2
  exit 1
fi
grep -q "^$token	assistant	Assistant$" "$DOMAIN_ROOT/sessions.tsv"

echo "=== env clear removes the session for the current token ==="
out=$(WF_TOKEN="$token" "$WF" env clear)
[ "$out" = "unset WF_TOKEN" ]
if grep -q "^$token	assistant	Assistant$" "$DOMAIN_ROOT/sessions.tsv"; then
  echo "FAIL: env clear left the session behind" >&2
  exit 1
fi

echo "=== exec passes WF_TOKEN to the child and cleans it up afterwards ==="
out=$("$WF" exec assistant -- env)
token=$(printf '%s\n' "$out" | sed -n "s/^WF_TOKEN=\([0-9a-f][0-9a-f]*\)$/\1/p")
if [ -z "$token" ]; then
  echo "FAIL: expected child env output to contain WF_TOKEN=<hex>" >&2
  exit 1
fi
if grep -q "^$token	assistant	Assistant$" "$DOMAIN_ROOT/sessions.tsv"; then
  echo "FAIL: exec left its temporary session behind" >&2
  exit 1
fi

echo "=== bare exec starts a shell-like child and still cleans it up afterwards ==="
cat > "$TESTROOT/fake-shell.sh" <<'EOF'
#!/bin/sh
printf 'WF_TOKEN=%s\n' "$WF_TOKEN"
exit 0
EOF
chmod +x "$TESTROOT/fake-shell.sh"
out=$(SHELL="$TESTROOT/fake-shell.sh" "$WF" exec assistant)
token=$(printf '%s\n' "$out" | sed -n "s/^WF_TOKEN=\([0-9a-f][0-9a-f]*\)$/\1/p")
if [ -z "$token" ]; then
  echo "FAIL: expected bare exec child to inherit WF_TOKEN" >&2
  exit 1
fi
if grep -q "^$token	assistant	Assistant$" "$DOMAIN_ROOT/sessions.tsv"; then
  echo "FAIL: bare exec left its temporary session behind" >&2
  exit 1
fi

echo "=== user env export requires a TTY for password input ==="
printf 'user	User	%s	%s\n' \
  '0123456789abcdef0123456789abcdef' \
  '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef' \
  >> "$DOMAIN_ROOT/users.tsv"
out=$(setsid "$WF" env export user 2>&1 || true)
printf '%s\n' "$out" | grep -q '^/dev/tty:'

echo "=== exec requires -- before child commands ==="
out=$("$WF" exec assistant env 2>&1 || true)
printf '%s\n' "$out" | grep -q "child commands require '--' before COMMAND"

echo "=== user create fails for an existing account ==="
out=$("$WF" user create assistant Assistant 2>&1 || true)
printf '%s\n' "$out" | grep -q '^user already exists: assistant$'

echo "15_auth_exec_env.sh: OK"