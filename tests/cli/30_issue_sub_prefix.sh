#!/bin/sh
# Issue subcommand prefix matching + ISSUE_ID prefix resolution.
# Requires being able to create a temporary domain and fake a login.

set -e
WF=${WF:-../../src/wf}

case "$WF" in
  /*) ;;
  *) WF="$(cd "$(dirname "$WF")" && pwd)/$(basename "$WF")" ;;
esac

# Work in an isolated temp dir so we control the cwd (thus the domain).
TESTROOT=$(mktemp -d)
trap 'rm -rf "$TESTROOT"' EXIT

cd "$TESTROOT"
export XDG_DATA_HOME="$TESTROOT/xdg"

# Initialize domain (creates the issues dir etc.)
"$WF" domain create >/dev/null 2>&1 || true

# Find the domain root that was just created for this cwd
DOMAIN_ROOT=$(find "$XDG_DATA_HOME" -type d -path '*/wf/domains/*' | head -1)
if [ -z "$DOMAIN_ROOT" ]; then
  echo "Could not locate domain root under $XDG_DATA_HOME" >&2
  exit 1
fi
ISSUES="$DOMAIN_ROOT/issues"

# Create a fake assistant session (bypass real auth) using printf for portability
# (some /bin/sh do not treat echo -e as bash does).
printf 'assistant\tAssistant\t\t\n' > "$DOMAIN_ROOT/users.tsv"
TOKEN="FAKETOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTOKTO"
printf '%s\tassistant\tAssistant\n' "$TOKEN" > "$DOMAIN_ROOT/sessions.tsv"

export WF_TOKEN="$TOKEN"

# Create issues with controllable prefixes for unique/ambiguous testing.
ID1="0123456789abcde0"
ID2="01fedcba98765432"   # shares "01" prefix with ID1 -> ambiguous at 2 chars
ID3="abc1111111111111"

# Minimal but valid enough .tsv so that load succeeds for show etc.
# Format from the code: id\tcontent\creator\tstatus\tapprover\tapproval_comment\tcreated\tupdated
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$ID1" "first issue content" "assistant" "open" "" "" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$ISSUES/$ID1.tsv"

printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$ID2" "second issue content" "assistant" "open" "" "" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$ISSUES/$ID2.tsv"

printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$ID3" "third issue content" "assistant" "open" "" "" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$ISSUES/$ID3.tsv"

echo "=== issue subcommand prefix: 'li' for list ==="
# List is a smoke test; protect from set -e because load errors on any stray files would fail the grep.
"$WF" i li 2>&1 | cat || true
echo "(list exercised)"

echo "=== issue subcommand prefix + ID prefix: 'sh 0123' (unique to ID1) ==="
"$WF" i sh 0123 2>&1 | grep -q "first issue content"

echo "=== ID prefix 2 chars is now ambiguous (ID1 + ID2 share '01') ==="
if "$WF" i show 01 2>&1 | grep -q "ambiguous issue id"; then
  :
else
  echo "expected ambiguous issue id for '01'" >&2
  exit 1
fi

echo "=== 4-char '0123' is unique to ID1 ==="
"$WF" i show 0123 2>&1 | grep -q "first issue content"

echo "=== 15-char prefix of ID1 is unique ==="
"$WF" i show 0123456789abcde 2>&1 | grep -q "first issue content"

echo "=== 1-char ID prefix is rejected as too short ==="
if "$WF" i show 0 2>&1 | grep -q "issue id too short"; then
  :
else
  echo "expected short issue id error for '0'" >&2
  exit 1
fi

echo "=== issue subcommand ambiguity: 'c' (create vs comment) ==="
if "$WF" i c something 2>&1 | grep -q "ambiguous issue command: c"; then
  :
else
  echo "expected ambiguous issue command for 'c'" >&2
  exit 1
fi

echo "=== issue subcommand ambiguity: 'approve-' (approve-with vs approve-partial) ==="
if "$WF" i approve- something 2>&1 | grep -q "ambiguous issue command: approve-"; then
  :
else
  echo "expected ambiguous issue command for 'approve-'" >&2
  exit 1
fi

echo "=== shortest unique prefix for approve-with dispatches ==="
out=$("$WF" i approve-w "$ID1" "only after canary passes" "review override" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected approve-w to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== exact approve command still dispatches despite approve-with siblings ==="
out=$("$WF" i approve "$ID1" "looks fine" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected approve to dispatch exactly" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for request-condition dispatches ==="
out=$("$WF" i request-c "$ID1" "only after canary passes" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected request-c to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only Assistant can update requested conditions"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for clear-condition dispatches ==="
out=$("$WF" i clear-c "$ID1" "scope changed" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected clear-c to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only Assistant can update requested conditions"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for approve-partial dispatches ==="
out=$("$WF" i approve-p "$ID1" "approve canary only" "review override" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected approve-p to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for reject-with dispatches ==="
out=$("$WF" i reject-w "$ID1" "reject until signoff" "review override" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected reject-w to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== exact reject command still dispatches despite reject-with sibling ==="
out=$("$WF" i reject "$ID1" "not acceptable" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected reject to dispatch exactly" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for hold-with dispatches ==="
out=$("$WF" i hold-w "$ID1" "hold until window opens" "review override" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected hold-w to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== exact hold command still dispatches despite hold-with sibling ==="
out=$("$WF" i hold "$ID1" "needs more review" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected hold to dispatch exactly" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== shortest unique prefix for invalidate-condition dispatches ==="
out=$("$WF" i invalidate- "$ID1" "reject requested condition" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected invalidate- to dispatch uniquely" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"no requested condition to invalidate"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== exact invalidate command still dispatches despite invalidate-condition sibling ==="
out=$("$WF" i invalidate "$ID1" "superseded work item" 2>&1 || true)
case "$out" in
  *"ambiguous issue command"*|*"unknown issue command"*) echo "expected invalidate to dispatch exactly" >&2; exit 1 ;;
  *"permission denied: only User can change approval status"*|*"usage:"*) : ;;
  *) printf '  (got: %s)\n' "$out" ;;
esac

echo "=== 'cr' create via prefix (assistant can create) ==="
NEWID=$("$WF" i cr "created via prefix subcommand" 2>&1)
echo "$NEWID" | grep -qE '^[0-9a-f]{16}$'

echo "=== show the newly created issue with short prefix ==="
"$WF" i sh "$(echo "$NEWID" | cut -c1-4)" 2>&1 | grep -q "created via prefix subcommand"

echo "=== unknown issue subcommand ==="
if "$WF" i xyzzy 2>&1 | grep -q "unknown issue command: xyzzy"; then
  :
else
  echo "expected unknown issue command" >&2
  exit 1
fi

echo "30_issue_sub_prefix.sh: OK"
