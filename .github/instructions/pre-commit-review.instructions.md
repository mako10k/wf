---
description: "Use when preparing a commit, asking whether changes are ready to commit, doing final review, or checking commit readiness. Before commit, run both the symmetry-consistency-coverage reviewer and the blunt non-expert user-eye reviewer against the changed files, then summarize blockers first."
---
# Pre-Commit Review Gate

- Before declaring a change ready to commit, run both shared review agents:
  - Review Symmetry Consistency Coverage
  - Review User Eye Blunt
- Example prompts:
  - `Review the changed files with Review Symmetry Consistency Coverage.`
  - `Review the changed files with Review User Eye Blunt.`
- Scope the review to the changed files when possible.
- Treat findings from either reviewer as blockers until they are either fixed or explicitly accepted by the user.
- Report findings first, ordered by severity.
- Normalize the blunt user-eye review into the severity-ordered summary yourself: treat clear first-use confusion, misleading wording, hidden prerequisites, and likely user mistakes as blockers when merging the two review outputs.
- If no findings remain, state that both review passes found no blocking issues.
- Do not skip the user-eye review just because tests pass.
- Do not skip the symmetry/coverage review just because behavior appears correct in one path.