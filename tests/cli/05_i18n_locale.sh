#!/bin/sh

set -e

WF=${WF:-../../src/wf}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LOCALEDIR="$ROOT/po/locale"

TESTROOT=$(mktemp -d)
trap 'rm -rf "$TESTROOT"' EXIT

if command -v msgfmt >/dev/null 2>&1; then
  LOCALEDIR="$TESTROOT/locale"
  mkdir -p "$LOCALEDIR/ja/LC_MESSAGES"
  msgfmt -c -o "$LOCALEDIR/ja/LC_MESSAGES/wf.mo" "$ROOT/po/ja.po"
elif [ ! -f "$LOCALEDIR/ja/LC_MESSAGES/wf.mo" ]; then
  echo "SKIP: compiled Japanese catalog not found at $LOCALEDIR/ja/LC_MESSAGES/wf.mo"
  exit 0
fi

if ! locale -a | grep -qi '^ja_JP\.utf-\?8$'; then
  echo "SKIP: ja_JP.UTF-8 locale is not available on this system"
  exit 0
fi

echo "=== localized help uses Japanese catalog when WF_LOCALEDIR is set ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help 2>&1)
printf '%s\n' "$out" | grep -q "エンティティ:"
printf '%s\n' "$out" | grep -q "ヘルプ:"
printf '%s\n' "$out" | grep -q "使い方: wf COMMAND \[ARGS...\]"
if printf '%s\n' "$out" | grep -q "^usage: wf COMMAND \[ARGS...\]"; then
  echo "top-level help still contains untranslated overview usage"
  exit 1
fi

echo "=== localized concepts help uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help concepts 2>&1)
printf '%s\n' "$out" | grep -q "概念:"
printf '%s\n' "$out" | grep -q "省略規則:"
printf '%s\n' "$out" | grep -q "分離境界です"
if printf '%s\n' "$out" | grep -q "無効化する requested condition がありません"; then
  echo "concepts help still contains stale mistranslation"
  exit 1
fi

echo "=== localized usecases help uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help usecases 2>&1)
printf '%s\n' "$out" | grep -q "よくある使い方:"
printf '%s\n' "$out" | grep -q "WF_EXEC_USER と WF_EXEC_DOMAIN も export"
if printf '%s\n' "$out" | grep -q "^      # wf exec also exports WF_EXEC_USER and WF_EXEC_DOMAIN$"; then
  echo "usecases help still contains untranslated wf exec export note"
  exit 1
fi

echo "=== localized semantics help uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help semantics 2>&1)
printf '%s\n' "$out" | grep -q "意味規則:"
printf '%s\n' "$out" | grep -q "DOMAIN の選択"

echo "=== localized user and domain help use Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help user 2>&1)
printf '%s\n' "$out" | grep -q "使い方: wf user COMMAND"
printf '%s\n' "$out" | grep -q "エンティティ: user"

out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" help domain 2>&1)
printf '%s\n' "$out" | grep -q "使い方: wf domain COMMAND"
printf '%s\n' "$out" | grep -q "'domain' の説明は 'wf help concepts' と 'wf help semantics' を参照してください。"

echo "=== localized issue help uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" issue --help 2>&1)
printf '%s\n' "$out" | grep -q "使い方: wf issue COMMAND \[ARGS...\]"
printf '%s\n' "$out" | grep -q "requested condition が有効なら、その条件で承認します"
if printf '%s\n' "$out" | grep -q "^usage: wf issue COMMAND \[ARGS...\]"; then
  echo "issue help still contains untranslated usage"
  exit 1
fi

echo "=== localized error uses Japanese catalog when WF_LOCALEDIR is set ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" xyzzy 2>&1 || true)
printf '%s\n' "$out" | grep -q "不明なコマンド"

echo "=== localized ambiguous command error uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" e 2>&1 || true)
printf '%s\n' "$out" | grep -q "曖昧なコマンド"

echo "=== localized user/domain subcommand errors use Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" user xyzzy 2>&1 || true)
printf '%s\n' "$out" | grep -q "不明な user command"

out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" domain c 2>&1 || true)
printf '%s\n' "$out" | grep -q "曖昧な domain コマンド"

out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" domain xyzzy 2>&1 || true)
printf '%s\n' "$out" | grep -q "不明な domain command"

echo "=== localized issue auth error uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" issue xyzzy 2>&1 || true)
printf '%s\n' "$out" | grep -q "ログインしていません"

echo "=== localized exec shell banners use Japanese catalog ==="
cat > "$TESTROOT/fake-shell.sh" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$TESTROOT/fake-shell.sh"
(
  cd "$TESTROOT"
  XDG_DATA_HOME="$TESTROOT/xdg" "$WF" domain create >/dev/null 2>&1 || true
  LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" \
    XDG_DATA_HOME="$TESTROOT/xdg" SHELL="$TESTROOT/fake-shell.sh" \
    "$WF" exec assistant >/dev/null 2>"$TESTROOT/exec-banner.err"
  LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" \
    XDG_DATA_HOME="$TESTROOT/xdg" WF_EXEC_USER=outer WF_EXEC_DOMAIN=outerdom \
    SHELL="$TESTROOT/fake-shell.sh" \
    "$WF" exec assistant >/dev/null 2>"$TESTROOT/exec-banner-nested.err"
)
grep -q '^wf exec shell: user=assistant domain=' "$TESTROOT/exec-banner.err"
grep -q '^wf exec shell: 戻りました: user=assistant domain=.* exit=0$' "$TESTROOT/exec-banner.err"
grep -q '^wf exec shell: user を切り替えました: outer@outerdom -> assistant@' "$TESTROOT/exec-banner-nested.err"
grep -q '^wf exec shell: user を復元しました: assistant@.* -> outer@outerdom exit=0$' "$TESTROOT/exec-banner-nested.err"
if grep -q '^wf exec shell returned:\|^wf exec shell restored user:\|^wf exec shell: switched user:' \
  "$TESTROOT/exec-banner.err" "$TESTROOT/exec-banner-nested.err"; then
  echo "exec shell banners still contain untranslated English text"
  exit 1
fi

echo "05_i18n_locale.sh: OK"
