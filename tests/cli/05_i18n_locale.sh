#!/bin/sh

set -e

WF=${WF:-../../src/wf}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LOCALEDIR="$ROOT/po/locale"

if [ ! -f "$LOCALEDIR/ja/LC_MESSAGES/wf.mo" ]; then
  echo "SKIP: compiled Japanese catalog not found at $LOCALEDIR/ja/LC_MESSAGES/wf.mo"
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

echo "=== localized issue auth error uses Japanese catalog ==="
out=$(LANG=ja_JP.UTF-8 LC_ALL=ja_JP.UTF-8 LANGUAGE=ja WF_LOCALEDIR="$LOCALEDIR" "$WF" issue xyzzy 2>&1 || true)
printf '%s\n' "$out" | grep -q "ログインしていません"

echo "05_i18n_locale.sh: OK"