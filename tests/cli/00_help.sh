#!/bin/sh
# Basic help and usage tests

set -e
WF=${WF:-../../src/wf}

echo "=== bare invocation (no args) shows usage on stderr ==="
"$WF" 2>&1 | grep -q "usage: .* COMMAND"
"$WF" 2>&1 | grep -q "Topics: concepts, semantics, usecases"

echo "=== --help / -h work ==="
"$WF" --help 2>&1 | grep -q "wf --version"
"$WF" -h 2>&1 | grep -q "wf version"

echo "=== help as subcommand also works at top level ==="
"$WF" help 2>&1 | grep -q "usage:"

echo "00_help.sh: OK"
