SUBDIRS = src

EXTRA_DIST = docs/requirements.md tests

# --- Testing ----------------------------------------------------------
# Run the full test suite with the standard "make check".
check-local: test

.PHONY: test test-unit test-cli

test: test-unit test-cli

test-unit:
	@echo "=== Unit tests (matcher etc.) ==="
	@mkdir -p tests/tmp
	$(CC) -std=c11 -Wall -Wextra -I. -Isrc -o tests/tmp/test_match \
	  tests/unit/test_match.c src/util.c -lcrypto
	@tests/tmp/test_match
	@echo "Unit tests OK."

test-cli:
	@echo "=== CLI / integration tests ==="
	@cd tests && WF="$$(pwd)/../src/wf" ./run.sh

# --- Developer quality targets ----------------------------------------
.PHONY: lint complexity clones

lint:
	@echo "=== Lint ==="
	@if command -v cppcheck >/dev/null 2>&1; then \
	  cppcheck --enable=all --error-exitcode=1 --quiet --inline-suppr \
	    --suppressions-list=.cppcheck-suppressions src 2>&1 || true; \
	else \
	  echo "cppcheck not installed (apt install cppcheck or equivalent)"; \
	fi
	@if command -v clang-tidy >/dev/null 2>&1; then \
	  clang-tidy -quiet -p . src/*.c -- -std=c11 -I. -Isrc || true; \
	else \
	  echo "clang-tidy not installed"; \
	fi
	@echo "Lint target finished (best effort)."

complexity:
	@echo "=== Cyclomatic complexity (lower is better) ==="
	@if command -v pmccabe >/dev/null 2>&1; then \
	  pmccabe -v src/*.c | sort -n; \
	else \
	  echo "pmccabe not installed."; \
	  echo "  Debian/Ubuntu: apt install pmccabe"; \
	  echo "  Or use lizard (pip install lizard) as alternative."; \
	  echo "Rough manual count of control keywords:"; \
	  grep -E '\b(if|for|while|switch|case|&&|\|\|)\b' src/*.c | wc -l || true; \
	fi

clones:
	@echo "=== Duplicate code (clones) ==="
	@if command -v duplo >/dev/null 2>&1; then \
	  duplo src/*.c; \
	else \
	  echo "duplo not installed for C/C++ clone detection."; \
	  echo "  Build from https://github.com/dlidstrom/Duplo or use jscpd (npm)."; \
	  echo "Simple heuristic (identical lines > 5):"; \
	  sort src/*.c | uniq -d | head -20 || true; \
	fi

