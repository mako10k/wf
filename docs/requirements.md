# wf Requirements

`wf` は、人間の User と AI の Assistant の間で Issue ベースのワークフローを管理する軽量 C コマンドである。

User は承認権限を持つ人間として扱う。人間であることを確認するため、User のログインには TTY からのパスワード入力を必須とする。

Assistant は AI の実行主体として扱う。Assistant はワークフロー上で Issue を作成・更新するための主体であり、パスワード保護は不要とする。

## Data Model

### Actor

- username: string
- role: `"User"` | `"Assistant"`
- token: `WF_TOKEN` (login 時に生成)

### User

- role: `"User"`
- password_hash: string
- password input: TTY required
- permissions:
  - Issue の read
  - Issue への comment
  - Issue の approve / reject / hold / resume

### Assistant

- role: `"Assistant"`
- password_hash: empty
- password input: none
- permissions:
  - Issue の create
  - Issue の read
  - 自分が作成した Issue の update / delete
  - Issue への comment

### Issue

- id: string
- content: string
- creator: Actor (Assistant のみ)
- status: `"open"` | `"approved"` | `"rejected"` | `"closed"` | `"hold"`
- approver: Actor (User のみ)
- approval_comment: string
- created_at: datetime
- updated_at: datetime

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
| Approve | ✓ | ✗ |
| Reject | ✓ | ✗ |
| Hold | ✓ | ✗ |
| Resume | ✓ | ✗ |
| Comment | ✓ | ✓ |

## Authentication

```bash
# User registration / password update
wf passwd
wf passwd USERNAME User

# Assistant registration (no password)
wf passwd assistant Assistant

# User login (TTY password required)
eval `wf login`
eval `wf login USERNAME`

# Assistant login (no password)
eval `wf login assistant`

# Logout
eval `wf logout`
```

Rules:

- `wf passwd` defaults to `wf passwd user User`.
- `wf login` defaults to `wf login user`.
- User login must read the password from TTY.
- Assistant login must not ask for a password.
- `wf login assistant` may auto-register the default Assistant if it does not exist.
- `wf logout` prints shell code that removes `WF_TOKEN`.

Command abbreviation:

- Top-level commands (`passwd`, `login`, `logout`, `issue`) and `issue` subcommands may be abbreviated to a unique prefix (e.g. `wf logi`, `wf i cr`, `wf i app`).
- On ambiguity the command prints "ambiguous command: ..." (or "ambiguous issue command: ...") and usage.

## Issue Commands

```bash
# Issue CRUD
wf issue create CONTENTS
wf issue list
wf issue show ISSUE_ID
wf issue update ISSUE_ID CONTENTS
wf issue delete ISSUE_ID

# Issue status management (User only)
wf issue approve ISSUE_ID COMMENT
wf issue reject ISSUE_ID COMMENT
wf issue hold ISSUE_ID COMMENT
wf issue resume ISSUE_ID COMMENT

# Issue comments
wf issue comment ISSUE_ID COMMENT
wf issue comments ISSUE_ID

# Issue search
wf issue search KEYWORD
```

- `issue` subcommands may be abbreviated to a unique prefix (e.g. `wf i sh 0123`, `wf i up 0123 new text`).
- `ISSUE_ID` may be given as a unique prefix of 2 or more characters (the tool resolves it against existing issues in the domain; ambiguous or too-short prefixes are reported with a clear error).

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
