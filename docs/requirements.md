# wf Requirements

`wf` は、人間の User と AI の Assistant の間で Issue ベースのワークフローを管理する軽量 C コマンドである。

User は承認権限を持つ人間として扱う。人間であることを確認するため、User のログインには TTY からのパスワード入力を必須とする。

Assistant は AI の実行主体として扱う。Assistant はワークフロー上で Issue を作成・更新するための主体であり、パスワード保護は不要とする。

## Data Model

### Actor

- username: string
- role: `"User"` | `"Assistant"`
- token: `WF_TOKEN` (`wf exec` / `wf env export` で生成)

### User

- role: `"User"`
- password_hash: string
- password input: TTY required
- permissions:
  - Issue の read
  - Issue への comment
  - 発行者が提示した承認依頼条件の read
  - 承認依頼条件に対する approve / reject / hold / resume / invalidate
  - 自分の条件を提示した上での conditional approval / partial approval / reject / hold

### Assistant

- role: `"Assistant"`
- password_hash: empty
- password input: none
- permissions:
  - Issue の create
  - Issue の read
  - 自分が作成した Issue の update / delete
  - Issue への承認依頼条件の設定 / 更新 / 取消
  - Issue への comment

### Issue

- id: string
- content: string
- creator: Actor (Assistant のみ)
- issue_state: `"open"` | `"approved"` | `"rejected"` | `"hold"` | `"invalid"`
- requested_condition: string
- requested_condition_state: `"none"` | `"active"` | `"invalid"`
- approver: Actor (User のみ)
- decision_type: `"none"` | `"approve"` | `"reject"` | `"hold"`
- decision_scope: `"none"` | `"full"` | `"partial"`
- decision_basis: `"none"` | `"as_requested"` | `"reviewer_override"`
- decision_condition: string
- decision_comment: string
- created_at: datetime
- updated_at: datetime

Issue の承認依頼条件と承認判断は分離して管理する。

- `requested_condition` は、発行者が「この条件で承認してほしい」と提示する条件本文である。
- `requested_condition_state=active` は現行の承認依頼条件であることを示す。
- `requested_condition_state=invalid` は承認者が発行者提示条件を不適切または無効と判断したことを示す。
- `decision_type=none` はまだ承認判断が確定していないことを示す。
- `decision_scope=none` は承認範囲が未確定であることを示す。
- `decision_basis=none` は発行者条件を採用した判断でも、承認者上書き条件でもないことを示す。
- `decision_basis=as_requested` は発行者提示条件をそのまま採用した判断である。
- `decision_basis=reviewer_override` は承認者が別条件を提示して判断したことを示す。
- `decision_scope=partial` は部分承認・部分否認のように対象範囲が限定される判断を示す。
- `issue_state=invalid` は Issue 自体が無効であり、承認フローを継続しないことを示す。
- Legacy data may still contain `closed`; current CLI treats it as a terminal read-only state and does not create it.

Current issue TSV format:

```text
id\tcontent\tcreator\tissue_state\trequested_condition\trequested_condition_state\tapprover\tdecision_type\tdecision_scope\tdecision_basis\tdecision_condition\tdecision_comment\tcreated_at\tupdated_at
```

- 実装は旧 8 列形式の issue TSV を読み取り可能であること。
- 旧形式から読み込んだ issue は、新形式で保存し直した時点で新列へ移行される。

### Comment

- issue_id: string
- author: Actor
- content: string
- created_at: datetime

## Permissions

| Operation | User | Assistant |
|-----------|------|-----------|
| Create Issue | ✗ | ✓ |
| Read Issue | ✓ | ✓ |
| Update Issue | ✗ | ✓ (own only) |
| Delete Issue | ✗ | ✓ (own only) |
| Request / update requested condition | ✗ | ✓ (own only) |
| Clear requested condition | ✗ | ✓ (own only) |
| Approve / reject / hold / resume | ✓ | ✗ |
| Reviewer-supplied condition for approve / reject / hold | ✓ | ✗ |
| Partial approval | ✓ | ✗ |
| Invalidate requested condition | ✓ | ✗ |
| Invalidate issue | ✓ | ✗ |
| Comment | ✓ | ✓ |

## Authentication

```bash
# User registration / password update
wf user passwd USERNAME User

# User creation without overwriting an existing account
wf user create USERNAME User

# Assistant registration (no password)
wf user passwd assistant Assistant
wf user create BOT Assistant

# Export to current shell (low-level primitive)
eval "$(wf env export USERNAME)"
eval "$(wf env clear)"

# Run one command with a temporary session
wf exec USERNAME -- COMMAND [ARGS...]

# Start an interactive shell with a temporary session
wf exec USERNAME
```

Rules:

- `wf user create` is create-only and fails if the username already exists.
- `wf user passwd` may create a new user or update an existing user's credentials/role.
- `wf env export` defaults to `wf env export user`.
- `wf exec` defaults to `wf exec user`.
- The default `user` account is not auto-created; register it first with `wf user passwd user User` before relying on the default.
- User `wf exec` / `wf env export` must read the password from TTY.
- Assistant `wf exec assistant` / `wf env export assistant` must not ask for a password.
- The reserved username `assistant` is auto-created on first use. Other Assistant names must be created explicitly.
- `wf exec assistant` / `wf env export assistant` may auto-register the default Assistant if it does not exist.
- `wf env clear` prints shell code that removes `WF_TOKEN`.
- `wf exec` requires `--` before a child command. Without a child command it starts an interactive shell.
- Interactive `wf exec` prints a one-line banner to stderr when the shell starts and another when the shell exits and control returns to `wf`.
- Interactive `wf exec` exports `WF_EXEC_USER` and `WF_EXEC_DOMAIN` for shell-side opt-in customizations.
- `WF_EXEC_DOMAIN` contains the current domain id, not a path or human-readable label.
- `wf exec` creates a temporary session, runs one command or one interactive shell, then removes that session automatically when the child process exits.

Command abbreviation:

- Full commands are the primary public interface. Abbreviations are convenience shortcuts.
- Top-level commands (`exec`, `env`, `version`, `user`, `issue`, `domain`) and entity subcommands under `env`, `user`, `issue`, and `domain` may be abbreviated to a unique prefix (e.g. `wf ex`, `wf en ex`, `wf ver`, `wf us pas`, `wf i cr`, `wf dom cur`).
- On ambiguity the command prints "ambiguous command: ..." (or "ambiguous issue command: ...") and usage.

## Issue Commands

```bash
# Issue CRUD
wf issue create CONTENTS
wf issue list
wf issue show ISSUE_ID
wf issue update ISSUE_ID CONTENTS
wf issue delete ISSUE_ID

# Requested condition management (Assistant only, own issue)
wf issue request-condition ISSUE_ID CONDITION
wf issue clear-condition ISSUE_ID REASON

# Issue review / decision management (User only)
wf issue approve ISSUE_ID COMMENT
wf issue approve-with ISSUE_ID CONDITION COMMENT
wf issue approve-partial ISSUE_ID CONDITION COMMENT
wf issue reject ISSUE_ID COMMENT
wf issue reject-with ISSUE_ID CONDITION COMMENT
wf issue hold ISSUE_ID COMMENT
wf issue hold-with ISSUE_ID CONDITION COMMENT
wf issue resume ISSUE_ID COMMENT
wf issue invalidate-condition ISSUE_ID REASON
wf issue invalidate ISSUE_ID REASON

# Issue comments
wf issue comment ISSUE_ID COMMENT
wf issue comments ISSUE_ID

# Issue search
wf issue search KEYWORD
```

- When `CONDITION`, `COMMENT`, or `REASON` contains spaces, quote it as a single shell argument.
- Example: `wf issue approve-with 0123abcd "only after canary passes" "looks acceptable"`

- `issue` subcommands may be abbreviated to a unique prefix (e.g. `wf i sh 0123`, `wf i up 0123 new text`).
- `ISSUE_ID` may be given as a unique prefix of 2 or more characters (the tool resolves it against existing issues in the domain; ambiguous or too-short prefixes are reported with a clear error).

Review semantics:

- 発行者は承認依頼条件を明示できる。条件がない場合は `requested_condition_state=none` とする。
- 承認者は `approve` / `reject` / `hold` により、発行者提示条件をそのまま採用するか、条件なしの判断を記録できる。
- 承認者は発行者提示条件がない、または適切でない場合、自分の条件を提示して判断できる。
- `approve` / `reject` / `hold` / `approve-with` / `reject-with` / `hold-with` / `approve-partial` は `open` 状態の Issue に対してのみ許可する。
- `request-condition` と `clear-condition` は `open` 状態の Issue に対してのみ許可する。terminal state (`invalid`, legacy `closed`) にも適用しない。
- 承認者は発行者提示条件のみを無効化できる。この場合 Issue 自体は継続してよい。
- `invalidate-condition` は未決定の Issue に対してのみ許可する。`hold` を `resume` して review decision をクリアした後は、再び使ってよい。
- 承認者は Issue 自体を無効化できる。この場合 `issue_state=invalid` とする。`requested_condition_state=active` だった場合は、その条件も無効扱いにする。
- `invalidate` は `approved` / `rejected` / `hold` からも許可する。Issue を無効化した時点で、その review decision は現行状態としては無効になるため、`decision_type` / `decision_scope` / `decision_basis` / `decision_condition` は `none` / 空文字へ戻し、無効化理由は `decision_comment` に残す。
- 承認者は部分承認を表現できる。部分承認の対象範囲や条件は `decision_condition` に記録する。
- `resume` は `hold` 状態の Issue に対してのみ許可する。
- `resume` すると current review decision はクリアされる。resume 理由は comment として残してよいが、`decision_comment` には残さない。
- `issue_state=invalid` になった Issue は、その後の review decision を受け付けない。
- terminal state (`invalid`, legacy `closed`) の Issue は current CLI では read-only とし、update/delete/request-condition/clear-condition/review/invalidate の対象にしない。
- 一覧表示と詳細表示は、少なくとも `issue_state`、`requested_condition_state`、`decision_type`、`decision_scope`、`decision_basis` を表示できること。

## Domain / Storage

`wf` stores data under XDG data directories and isolates data by command execution directory.

- Base directory:
  - `$XDG_DATA_HOME/wf/domains/<domain-id>/`
  - if `$XDG_DATA_HOME` is unset, `$HOME/.local/share/wf/domains/<domain-id>/`
- `domain-id` is SHA-256 of `wf:<real cwd>`.
- Each execution directory has independent:
  - users
  - sessions
  - issues
  - comments
- The domain model follows the directory-based scope idea from `mako10k/secdat`.

Current storage files:

```text
$XDG_DATA_HOME/wf/domains/<domain-id>/
  path
  users.tsv
  sessions.tsv
  issues/
    <issue-id>.tsv
    <issue-id>.comments.tsv
```

## Design Constraints

- Implemented in C.
- Keep the command lightweight.
- Keep responsibilities separated:
  - CLI dispatch
  - domain resolution
  - authentication/session handling
  - issue operations
  - storage utilities
- Do not require a server.
- Do not require a database.
- Store local workflow state only.
