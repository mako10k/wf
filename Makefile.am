SUBDIRS = src

LINGUAS = ja
PO_DIR = $(top_srcdir)/po
PO_BUILD_DIR = $(top_builddir)/po
COMPLETION_DIR = $(top_srcdir)/completions
JA_MO = $(PO_BUILD_DIR)/locale/ja/LC_MESSAGES/$(PACKAGE).mo
MO_FILES = $(JA_MO)

EXTRA_DIST = docs/requirements.md tests po/ja.po po/wf.pot completions/wf.bash

# --- Testing ----------------------------------------------------------
# Run the full test suite with the standard "make check".
check-local: test

.PHONY: test test-unit test-cli
.PHONY: locale pot update-po

locale: $(MO_FILES)

$(JA_MO): $(PO_DIR)/ja.po $(PO_DIR)/wf.pot
	@mkdir -p "$(PO_BUILD_DIR)/locale/ja/LC_MESSAGES"
	@if test "x$(MSGFMT)" = xfalse; then \
	  echo "msgfmt not found; install gettext to build translations." >&2; \
	  exit 1; \
	fi
	$(MSGFMT) -c -o "$@" "$<"

pot:
	@mkdir -p $(PO_DIR)
	@if test "x$(XGETTEXT)" = xfalse; then \
	  echo "xgettext not found; install gettext to regenerate po/wf.pot." >&2; \
	  exit 1; \
	fi
	$(XGETTEXT) --from-code=UTF-8 --keyword=_ --keyword=N_ --sort-output \
	  --package-name='$(PACKAGE)' --package-version='$(VERSION)' \
	  -o $(PO_DIR)/wf.pot $(top_srcdir)/src/*.c $(top_srcdir)/src/*.h

update-po: pot
	@if test "x$(MSGMERGE)" = xfalse; then \
	  echo "msgmerge not found; install gettext to update po/ja.po." >&2; \
	  exit 1; \
	fi
	$(MSGMERGE) --update $(PO_DIR)/ja.po $(PO_DIR)/wf.pot

test: locale test-unit test-cli

install-data-local: $(MO_FILES)
	@mkdir -p "$(DESTDIR)$(localedir)/ja/LC_MESSAGES"
	$(INSTALL_DATA) "$(PO_BUILD_DIR)/locale/ja/LC_MESSAGES/$(PACKAGE).mo" \
	  "$(DESTDIR)$(localedir)/ja/LC_MESSAGES/$(PACKAGE).mo"
	@mkdir -p "$(DESTDIR)$(datadir)/bash-completion/completions"
	$(INSTALL_DATA) "$(COMPLETION_DIR)/wf.bash" \
	  "$(DESTDIR)$(datadir)/bash-completion/completions/wf"

uninstall-local:
	rm -f "$(DESTDIR)$(localedir)/ja/LC_MESSAGES/$(PACKAGE).mo"
	rm -f "$(DESTDIR)$(datadir)/bash-completion/completions/wf"

clean-local:
	rm -rf $(PO_BUILD_DIR)/locale

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

