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
"$WF" --help 2>&1 | grep -q "wf exec \[USERNAME\]"
"$WF" --help 2>&1 | grep -q "wf env COMMAND"

echo "=== help as subcommand also works at top level ==="
"$WF" help 2>&1 | grep -q "usage:"

echo "=== issue help shows the expanded review command set ==="
"$WF" issue help 2>&1 | grep -q "approve-with ISSUE_ID CONDITION COMMENT"
"$WF" issue help 2>&1 | grep -q "approve-partial ISSUE_ID CONDITION COMMENT"
"$WF" issue help 2>&1 | grep -q "reject-with ISSUE_ID CONDITION COMMENT"
"$WF" issue help 2>&1 | grep -q "hold-with ISSUE_ID CONDITION COMMENT"
"$WF" issue help 2>&1 | grep -q "resume ISSUE_ID COMMENT"
"$WF" issue help 2>&1 | grep -q "invalidate-condition ISSUE_ID REASON"
"$WF" issue help 2>&1 | grep -q "invalidate ISSUE_ID REASON"

echo "=== wf help COMMAND routes to command help ==="
"$WF" help issue 2>&1 | grep -q "approve-with ISSUE_ID CONDITION COMMENT"
"$WF" help user 2>&1 | grep -q "usage: wf user COMMAND"
"$WF" help domain 2>&1 | grep -q "usage: wf domain COMMAND"
"$WF" help exec 2>&1 | grep -q "usage: wf exec \[USERNAME\]"
"$WF" help env 2>&1 | grep -q "usage: wf env COMMAND"

echo "=== wf COMMAND --help works for top-level commands ==="
"$WF" exec --help 2>&1 | grep -q "usage: wf exec \[USERNAME\]"
"$WF" env --help 2>&1 | grep -q "usage: wf env COMMAND"
"$WF" version --help 2>&1 | grep -q "usage: wf version"
"$WF" issue --help 2>&1 | grep -q "usage: wf issue COMMAND"
"$WF" user --help 2>&1 | grep -q "usage: wf user COMMAND"
"$WF" domain --help 2>&1 | grep -q "usage: wf domain COMMAND"

echo "00_help.sh: OK"
