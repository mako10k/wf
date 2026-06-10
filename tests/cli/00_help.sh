#!/bin/sh
# Basic help and usage tests

set -e
WF=${WF:-../../src/wf}

echo "=== bare invocation (no args) shows usage on stderr ==="
"$WF" 2>&1 | grep -q "usage: .* COMMAND"
"$WF" 2>&1 | grep -q "COMMAND may be abbreviated"

echo "=== --help / -h work ==="
"$WF" --help 2>&1 | grep -q "ISSUE_ID may be abbreviated"
"$WF" -h 2>&1 | grep -q "wf passwd"

echo "=== help as subcommand also works at top level ==="
"$WF" help 2>&1 | grep -q "usage:"

echo "00_help.sh: OK"
