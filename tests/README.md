# Tests for wf

## Running

After building the binary (`make -C src` or `make` after configure):

```sh
make check          # runs everything (unit + CLI)
make test           # same
make test-unit      # just the C unit tests (matcher, etc.)
make test-cli       # just the CLI / integration tests
```

You can also run directly:

```sh
WF=src/wf tests/run.sh
```

Individual scripts are executable:

```sh
WF=src/wf tests/cli/30_issue_sub_prefix.sh
```

## What is covered

- Unit tests for the core `wf_match_prefix` abstraction (used by both
  top-level subcommand dispatch and issue ID / subcommand resolution).
- CLI smoke tests for help/usage text.
- Unknown command and "ambiguous command" messages for top-level prefixes.
- Prefix dispatch for top-level commands (`ex`, `en ex`, `en cl`, and delimiter/ambiguity cases, ...).
- Full exercise of issue subcommand prefix matching + `ISSUE_ID` prefix
  resolution (including setting up a temporary domain + fake assistant
  session + multiple issues with controlled name prefixes for ambiguity tests).

## Adding more tests

- Put pure C unit tests under `tests/unit/`.
- Put CLI/integration scenarios under `tests/cli/*.sh`.
- The runner (`tests/run.sh`) discovers `cli/*.sh` and also builds/runs
  the unit binaries.

## Quality targets (developer)

```sh
make lint         # cppcheck + clang-tidy (best effort)
make complexity   # pmccabe (or rough grep fallback)
make clones       # duplo (or rough fallback)
```

These targets are intentionally non-fatal if the external tools are not installed.
Install the tools for stricter local checking:

- `cppcheck`, `clang-tidy` (for lint)
- `pmccabe` or `lizard` (for complexity)
- `duplo` or `jscpd` (for clone detection)
