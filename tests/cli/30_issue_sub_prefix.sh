#!/bin/sh
# Issue subcommand prefix matching + ISSUE_ID prefix resolution.
# Requires being able to create a temporary domain and fake a login.

set -e
WF=${WF:-../../src/wf}

# Work in an isolated temp dir so we control the cwd (thus the domain).
TESTROOT=$(mktemp -d)
trap 'rm -rf "$TESTROOT"' EXIT

cd "$TESTROOT"
export XDG_DATA_HOME="$TESTROOT/xdg"

# Initialize domain (creates the issues dir etc.)
"$WF" --help >/dev/null 2>&1 || true

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

echo "=== issue subcommand ambiguity: 'c' (create vs comment) ==="
if "$WF" i c something 2>&1 | grep -q "ambiguous issue command: c"; then
  :
else
  echo "expected ambiguous issue command for 'c'" >&2
  exit 1
fi

echo "=== 'cr' create via prefix (assistant can create) ==="
NEWID=$("$WF" i cr "created via prefix subcommand" 2>&1)
echo "$NEWID" | grep -qE '^[0-9a-f]{16}$'

echo "=== show the newly created issue with short prefix ==="
"$WF" i sh "${NEWID%?????????????????}" 2>&1 | grep -q "created via prefix subcommand" || \
  "$WF" i sh "$(echo "$NEWID" | cut -c1-4)" 2>&1 | grep -q "created via prefix subcommand"

echo "=== unknown issue subcommand ==="
if "$WF" i xyzzy 2>&1 | grep -q "unknown issue command: xyzzy"; then
  :
else
  echo "expected unknown issue command" >&2
  exit 1
fi

echo "30_issue_sub_prefix.sh: OK"
