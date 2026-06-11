#!/bin/sh

set -e

WF=${WF:-../../src/wf}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
COMPLETION="$ROOT/completions/wf.bash"

if ! command -v bash >/dev/null 2>&1; then
  echo "SKIP: bash not found"
  exit 0
fi

if [ ! -f "$COMPLETION" ]; then
  echo "completion script not found: $COMPLETION" >&2
  exit 1
fi

echo "=== top-level command completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf i)
  COMP_CWORD=1
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "issue"

echo "=== help topic completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf help se)
  COMP_CWORD=2
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "semantics"

echo "=== entity help completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf env h)
  COMP_CWORD=2
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "help"

echo "=== issue help topic completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf issue help se)
  COMP_CWORD=3
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "semantics"

echo "=== issue examples help completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf issue help ex)
  COMP_CWORD=3
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "examples"

echo "=== issue subcommand completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf issue app)
  COMP_CWORD=2
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "approve"

echo "=== user role completion ==="
bash -lc '
  set -e
  WF_BIN="$1"
  source "$2"
  COMP_WORDS=(wf user create alice A)
  COMP_CWORD=4
  COMP_WORDS[0]="$WF_BIN"
  _wf
  printf "%s\n" "${COMPREPLY[@]}"
' bash "$WF" "$COMPLETION" | grep -qx "Assistant"

echo "=== completion helper emits issue subcommands ==="
"$WF" __complete issue | grep -qx "approve-with"

echo "=== install and uninstall both mention completion file ==="
make -C "$ROOT" -n install-data-local uninstall-local | grep -q "bash-completion/completions/wf"

echo "=== install and uninstall both mention locale catalog ==="
make -C "$ROOT" -n install-data-local uninstall-local | grep -q "/ja/LC_MESSAGES/wf.mo"

echo "06_bash_completion.sh: OK"