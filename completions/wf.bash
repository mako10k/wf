_wf()
{
    local cur prev words cword
    if declare -F _init_completion >/dev/null 2>&1; then
        _init_completion -n : || return
    else
        COMPREPLY=()
        cur=${COMP_WORDS[COMP_CWORD]}
        prev=${COMP_WORDS[COMP_CWORD-1]}
        words=("${COMP_WORDS[@]}")
        cword=$COMP_CWORD
    fi

    local candidates
    local helper_args=()

    if [ "$cword" -gt 1 ]; then
        helper_args=("${words[@]:1:$((cword - 1))}")
    fi

    candidates=$("${words[0]}" __complete "${helper_args[@]}" 2>/dev/null) || return
    COMPREPLY=( $(compgen -W "$candidates" -- "$cur") )
}

complete -F _wf wf