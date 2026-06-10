#!/bin/sh
# Requested-condition and review decision flow tests.

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

ASSISTANT_TOKEN="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
REVIEWER_TOKEN="RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR"
OTHER_ASSISTANT_TOKEN="BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

printf 'assistant\tAssistant\t\t\nintruder\tAssistant\t\t\nreviewer\tUser\t\t\n' > "$DOMAIN_ROOT/users.tsv"
printf '%s\tassistant\tAssistant\n%s\tintruder\tAssistant\n%s\treviewer\tUser\n' "$ASSISTANT_TOKEN" "$OTHER_ASSISTANT_TOKEN" "$REVIEWER_TOKEN" > "$DOMAIN_ROOT/sessions.tsv"

echo "=== assistant can request and clear conditions ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE1=$("$WF" issue create "feature gate rollout")
"$WF" issue request-condition "$ISSUE1" "enable only behind gate"
"$WF" issue show "$ISSUE1" | grep -q '^requested_condition_state: active$'
"$WF" issue show "$ISSUE1" | grep -q '^requested_condition: enable only behind gate$'
"$WF" issue clear-condition "$ISSUE1" "scope changed"
"$WF" issue show "$ISSUE1" | grep -q '^requested_condition_state: none$'
"$WF" issue show "$ISSUE1" | grep -q '^requested_condition: $'
"$WF" issue comments "$ISSUE1" | grep -q 'requested_condition: enable only behind gate'
"$WF" issue comments "$ISSUE1" | grep -q 'requested_condition cleared: scope changed'
"$WF" issue list | awk -F '\t' -v id="$ISSUE1" '$1 == id && $3 == "none" && $4 == "none" && $5 == "none" && $6 == "none" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search feature | awk -F '\t' -v id="$ISSUE1" '$1 == id && $3 == "none" && $4 == "none" && $5 == "none" && $6 == "none" { found = 1 } END { exit(found ? 0 : 1) }'

echo "=== approve uses requested condition when active ==="
ISSUE2=$("$WF" issue create "production rollout")
"$WF" issue request-condition "$ISSUE2" "after monitoring dashboard is green"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue approve "$ISSUE2" "approved as requested"
"$WF" issue show "$ISSUE2" | grep -q '^issue_state: approved$'
"$WF" issue show "$ISSUE2" | grep -q '^decision_basis: as_requested$'
"$WF" issue show "$ISSUE2" | grep -q '^decision_scope: full$'
"$WF" issue show "$ISSUE2" | grep -q '^decision_condition: after monitoring dashboard is green$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE2" '$1 == id && $2 == "approved" && $3 == "active" && $4 == "approve" && $5 == "full" && $6 == "as_requested" && $7 == "assistant" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search production | awk -F '\t' -v id="$ISSUE2" '$1 == id && $2 == "approved" && $3 == "active" && $4 == "approve" && $5 == "full" && $6 == "as_requested" && $7 == "assistant" { found = 1 } END { exit(found ? 0 : 1) }'
export WF_TOKEN="$ASSISTANT_TOKEN"
out=$("$WF" issue request-condition "$ISSUE2" "must not edit approved issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update requested condition unless issue is open"
out=$("$WF" issue clear-condition "$ISSUE2" "must not clear approved issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot clear requested condition unless issue is open"

echo "=== role and ownership guards reject invalid callers ==="
export WF_TOKEN="$OTHER_ASSISTANT_TOKEN"
out=$("$WF" issue request-condition "$ISSUE1" "intruder edit" 2>&1 || true)
printf '%s\n' "$out" | grep -q "permission denied: Assistant can update own issues only"
out=$("$WF" issue approve "$ISSUE2" "assistant must not approve" 2>&1 || true)
printf '%s\n' "$out" | grep -q "permission denied: only User can change approval status"
export WF_TOKEN="$REVIEWER_TOKEN"
out=$("$WF" issue request-condition "$ISSUE1" "user must not request" 2>&1 || true)
printf '%s\n' "$out" | grep -q "permission denied: only Assistant can update requested conditions"

echo "=== plain review commands without requested condition record no basis ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE2B=$("$WF" issue create "plain approve path")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue approve "$ISSUE2B" "approved without requested condition"
"$WF" issue show "$ISSUE2B" | grep -q '^decision_basis: none$'
"$WF" issue show "$ISSUE2B" | grep -q '^decision_condition: $'

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE2C=$("$WF" issue create "plain reject path")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue reject "$ISSUE2C" "rejected without requested condition"
"$WF" issue show "$ISSUE2C" | grep -q '^decision_basis: none$'
"$WF" issue show "$ISSUE2C" | grep -q '^decision_condition: $'

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE2D=$("$WF" issue create "plain hold path")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue hold "$ISSUE2D" "held without requested condition"
"$WF" issue show "$ISSUE2D" | grep -q '^decision_basis: none$'
"$WF" issue show "$ISSUE2D" | grep -q '^decision_condition: $'

echo "=== reviewer override and partial approval are recorded ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE3=$("$WF" issue create "regional rollout")
"$WF" issue request-condition "$ISSUE3" "all regions at once"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue approve-with "$ISSUE3" "only after canary region passes" "override requested condition"
"$WF" issue show "$ISSUE3" | grep -q '^decision_basis: reviewer_override$'
"$WF" issue show "$ISSUE3" | grep -q '^decision_condition: only after canary region passes$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE3" '$1 == id && $2 == "approved" && $4 == "approve" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search regional | awk -F '\t' -v id="$ISSUE3" '$1 == id && $2 == "approved" && $4 == "approve" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue invalidate "$ISSUE3" "regional rollout replaced by new plan"
"$WF" issue show "$ISSUE3" | grep -q '^issue_state: invalid$'
"$WF" issue show "$ISSUE3" | grep -q '^requested_condition_state: invalid$'
"$WF" issue show "$ISSUE3" | grep -q '^decision_type: none$'
"$WF" issue show "$ISSUE3" | grep -q '^decision_scope: none$'
"$WF" issue show "$ISSUE3" | grep -q '^decision_basis: none$'
"$WF" issue show "$ISSUE3" | grep -q '^decision_comment: regional rollout replaced by new plan$'

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE4=$("$WF" issue create "batch job release")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue approve-partial "$ISSUE4" "approve dry-run path only" "full rollout still blocked"
"$WF" issue show "$ISSUE4" | grep -q '^decision_scope: partial$'
"$WF" issue show "$ISSUE4" | grep -q '^decision_basis: reviewer_override$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE4" '$1 == id && $2 == "approved" && $4 == "approve" && $5 == "partial" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search batch | awk -F '\t' -v id="$ISSUE4" '$1 == id && $2 == "approved" && $4 == "approve" && $5 == "partial" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'

echo "=== reject, hold, and resume flows are recorded ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE4B=$("$WF" issue create "regional rollback")
"$WF" issue request-condition "$ISSUE4B" "rollback only after alert review"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue reject "$ISSUE4B" "rejected as requested"
"$WF" issue show "$ISSUE4B" | grep -q '^issue_state: rejected$'
"$WF" issue show "$ISSUE4B" | grep -q '^decision_type: reject$'
"$WF" issue show "$ISSUE4B" | grep -q '^decision_basis: as_requested$'
"$WF" issue search rollback | awk -F '\t' -v id="$ISSUE4B" '$1 == id && $2 == "rejected" && $4 == "reject" && $6 == "as_requested" { found = 1 } END { exit(found ? 0 : 1) }'
out=$("$WF" issue approve "$ISSUE4B" "must not re-review rejected issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot change review decision unless issue is open"
export WF_TOKEN="$ASSISTANT_TOKEN"
out=$("$WF" issue request-condition "$ISSUE4B" "must not edit rejected issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update requested condition unless issue is open"

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE4C=$("$WF" issue create "maintenance window")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue reject-with "$ISSUE4C" "reject until customer signs off" "override reject condition"
"$WF" issue show "$ISSUE4C" | grep -q '^decision_type: reject$'
"$WF" issue show "$ISSUE4C" | grep -q '^decision_basis: reviewer_override$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE4C" '$1 == id && $2 == "rejected" && $4 == "reject" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search maintenance | awk -F '\t' -v id="$ISSUE4C" '$1 == id && $2 == "rejected" && $4 == "reject" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue invalidate "$ISSUE4C" "maintenance window cancelled"
"$WF" issue show "$ISSUE4C" | grep -q '^issue_state: invalid$'
"$WF" issue show "$ISSUE4C" | grep -q '^decision_type: none$'
"$WF" issue show "$ISSUE4C" | grep -q '^decision_comment: maintenance window cancelled$'

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE4D=$("$WF" issue create "service pause")
"$WF" issue request-condition "$ISSUE4D" "hold until freeze lifts"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue hold "$ISSUE4D" "held as requested"
"$WF" issue show "$ISSUE4D" | grep -q '^issue_state: hold$'
"$WF" issue show "$ISSUE4D" | grep -q '^decision_type: hold$'
"$WF" issue show "$ISSUE4D" | grep -q '^decision_basis: as_requested$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE4D" '$1 == id && $2 == "hold" && $4 == "hold" && $6 == "as_requested" { found = 1 } END { exit(found ? 0 : 1) }'
out=$("$WF" issue hold "$ISSUE4D" "must not re-review held issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot change review decision unless issue is open"
export WF_TOKEN="$ASSISTANT_TOKEN"
out=$("$WF" issue clear-condition "$ISSUE4D" "must not clear held issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot clear requested condition unless issue is open"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue resume "$ISSUE4D" "freeze lifted"
"$WF" issue show "$ISSUE4D" | grep -q '^issue_state: open$'
"$WF" issue show "$ISSUE4D" | grep -q '^decision_type: none$'
"$WF" issue show "$ISSUE4D" | grep -q '^decision_comment: $'
"$WF" issue search pause | awk -F '\t' -v id="$ISSUE4D" '$1 == id && $2 == "open" && $4 == "none" && $5 == "none" && $6 == "none" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue invalidate-condition "$ISSUE4D" "freeze policy is obsolete"
"$WF" issue show "$ISSUE4D" | grep -q '^requested_condition_state: invalid$'

out=$("$WF" issue resume "$ISSUE2" "should fail outside hold" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot resume issue unless it is on hold"

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE4E=$("$WF" issue create "capacity control")
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue hold-with "$ISSUE4E" "hold until off-peak window" "override hold condition"
"$WF" issue show "$ISSUE4E" | grep -q '^decision_type: hold$'
"$WF" issue show "$ISSUE4E" | grep -q '^decision_basis: reviewer_override$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE4E" '$1 == id && $2 == "hold" && $4 == "hold" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search capacity | awk -F '\t' -v id="$ISSUE4E" '$1 == id && $2 == "hold" && $4 == "hold" && $5 == "full" && $6 == "reviewer_override" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue invalidate "$ISSUE4E" "capacity control obsolete"
"$WF" issue show "$ISSUE4E" | grep -q '^issue_state: invalid$'
"$WF" issue show "$ISSUE4E" | grep -q '^decision_type: none$'
"$WF" issue show "$ISSUE4E" | grep -q '^decision_comment: capacity control obsolete$'

echo "=== requested condition and issue invalidation work ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE5=$("$WF" issue create "staging exception")
"$WF" issue request-condition "$ISSUE5" "skip backup just this once"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue invalidate-condition "$ISSUE5" "backup skip is not acceptable"
"$WF" issue show "$ISSUE5" | grep -q '^requested_condition_state: invalid$'
"$WF" issue comments "$ISSUE5" | grep -q 'requested_condition invalidated: backup skip is not acceptable'
"$WF" issue list | awk -F '\t' -v id="$ISSUE5" '$1 == id && $3 == "invalid" && $4 == "none" && $5 == "none" && $6 == "none" { found = 1 } END { exit(found ? 0 : 1) }'
"$WF" issue search staging | awk -F '\t' -v id="$ISSUE5" '$1 == id && $3 == "invalid" && $4 == "none" && $5 == "none" && $6 == "none" { found = 1 } END { exit(found ? 0 : 1) }'
out=$("$WF" issue invalidate-condition "$ISSUE5" "must not invalidate an already invalid condition" 2>&1 || true)
printf '%s\n' "$out" | grep -q "requested condition is already invalid"
export WF_TOKEN="$ASSISTANT_TOKEN"
"$WF" issue request-condition "$ISSUE5" "retry with monitored backup"
"$WF" issue show "$ISSUE5" | grep -q '^requested_condition_state: active$'
"$WF" issue show "$ISSUE5" | grep -q '^requested_condition: retry with monitored backup$'
"$WF" issue clear-condition "$ISSUE5" "fallback no longer needed"
"$WF" issue show "$ISSUE5" | grep -q '^requested_condition_state: none$'
export WF_TOKEN="$REVIEWER_TOKEN"

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE5A=$("$WF" issue create "as requested invalidation guard")
"$WF" issue request-condition "$ISSUE5A" "must be reviewed first"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue approve "$ISSUE5A" "approved once"
out=$("$WF" issue invalidate-condition "$ISSUE5A" "too late now" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot invalidate requested condition after a review decision"

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE5B=$("$WF" issue create "conditionless request")
export WF_TOKEN="$REVIEWER_TOKEN"
out=$("$WF" issue invalidate-condition "$ISSUE5B" "no condition present" 2>&1 || true)
printf '%s\n' "$out" | grep -q "no requested condition to invalidate"

export WF_TOKEN="$ASSISTANT_TOKEN"
ISSUE6=$("$WF" issue create "obsolete request")
"$WF" issue request-condition "$ISSUE6" "old condition before invalidation"
export WF_TOKEN="$REVIEWER_TOKEN"
"$WF" issue invalidate "$ISSUE6" "request superseded by new work item"
"$WF" issue show "$ISSUE6" | grep -q '^issue_state: invalid$'
"$WF" issue show "$ISSUE6" | grep -q '^requested_condition_state: invalid$'
"$WF" issue show "$ISSUE6" | grep -q '^decision_comment: request superseded by new work item$'
"$WF" issue list | awk -F '\t' -v id="$ISSUE6" '$1 == id && $2 == "invalid" && $3 == "invalid" && $4 == "none" { found = 1 } END { exit(found ? 0 : 1) }'
out=$("$WF" issue approve "$ISSUE6" "must not reopen invalid issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot review issue after it has reached a terminal state"
out=$("$WF" issue invalidate-condition "$ISSUE6" "must not touch invalid issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot invalidate requested condition after the issue reached a terminal state"
out=$("$WF" issue invalidate "$ISSUE6" "must not invalidate invalid issue again" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot invalidate issue after it has reached a terminal state"
export WF_TOKEN="$ASSISTANT_TOKEN"
out=$("$WF" issue request-condition "$ISSUE6" "must not touch invalid issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update requested condition after the issue reached a terminal state"
out=$("$WF" issue clear-condition "$ISSUE6" "must not clear invalid issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot clear requested condition after the issue reached a terminal state"
out=$("$WF" issue update "$ISSUE6" "must not update invalid issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update issue after it has reached a terminal state"
out=$("$WF" issue delete "$ISSUE6" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot delete issue after it has reached a terminal state"
export WF_TOKEN="$REVIEWER_TOKEN"

LEGACY_CLOSED="deadbeef12345678"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$LEGACY_CLOSED" "legacy closed" "assistant" "closed" "reviewer" "done" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$DOMAIN_ROOT/issues/$LEGACY_CLOSED.tsv"
out=$("$WF" issue approve "$LEGACY_CLOSED" "must not review closed issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot review issue after it has reached a terminal state"
export WF_TOKEN="$ASSISTANT_TOKEN"
out=$("$WF" issue request-condition "$LEGACY_CLOSED" "must not touch closed issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update requested condition after the issue reached a terminal state"
out=$("$WF" issue clear-condition "$LEGACY_CLOSED" "must not clear closed issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot clear requested condition after the issue reached a terminal state"
out=$("$WF" issue update "$LEGACY_CLOSED" "must not update closed issue" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot update issue after it has reached a terminal state"
out=$("$WF" issue delete "$LEGACY_CLOSED" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot delete issue after it has reached a terminal state"
export WF_TOKEN="$REVIEWER_TOKEN"
out=$("$WF" issue invalidate "$LEGACY_CLOSED" "must not invalidate closed issue again" 2>&1 || true)
printf '%s\n' "$out" | grep -q "cannot invalidate issue after it has reached a terminal state"

echo "=== old 8-column issues are migrated on save ==="
export WF_TOKEN="$ASSISTANT_TOKEN"
OLDID="deadc0de1234beef"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$OLDID" "legacy issue" "assistant" "open" "" "" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$DOMAIN_ROOT/issues/$OLDID.tsv"
"$WF" issue request-condition "$OLDID" "migrate on next save"
awk -F '\t' 'NF == 14 { found = 1 } END { exit(found ? 0 : 1) }' "$DOMAIN_ROOT/issues/$OLDID.tsv"
"$WF" issue show "$OLDID" | grep -q '^requested_condition: migrate on next save$'

LEGACY_APPROVED="feedface12345678"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$LEGACY_APPROVED" "legacy approved" "assistant" "approved" "reviewer" "done" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$DOMAIN_ROOT/issues/$LEGACY_APPROVED.tsv"
"$WF" issue update "$LEGACY_APPROVED" "legacy approved updated"
"$WF" issue show "$LEGACY_APPROVED" | grep -q '^decision_type: approve$'
"$WF" issue show "$LEGACY_APPROVED" | grep -q '^decision_scope: full$'

LEGACY_REJECTED="facefeed12345678"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$LEGACY_REJECTED" "legacy rejected" "assistant" "rejected" "reviewer" "blocked" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$DOMAIN_ROOT/issues/$LEGACY_REJECTED.tsv"
"$WF" issue update "$LEGACY_REJECTED" "legacy rejected updated"
"$WF" issue show "$LEGACY_REJECTED" | grep -q '^decision_type: reject$'
"$WF" issue show "$LEGACY_REJECTED" | grep -q '^decision_scope: full$'

LEGACY_HOLD="c0ffee0012345678"
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
  "$LEGACY_HOLD" "legacy hold" "assistant" "hold" "reviewer" "waiting" "2026-01-01T00:00:00Z" "2026-01-01T00:00:00Z" \
  > "$DOMAIN_ROOT/issues/$LEGACY_HOLD.tsv"
"$WF" issue update "$LEGACY_HOLD" "legacy hold updated"
"$WF" issue show "$LEGACY_HOLD" | grep -q '^decision_type: hold$'
"$WF" issue show "$LEGACY_HOLD" | grep -q '^decision_scope: full$'

echo "40_issue_review_conditions.sh: OK"