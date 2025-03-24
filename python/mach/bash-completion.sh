# Is bash completion too slow to your taste?
# Consider generating fast completion scripts with:
# ./mach mach-completion bash -f path/to/mach.bash-completion
# See https://firefox-source-docs.mozilla.org/mach/usage.html#tab-completion

function _mach()
{
  local cur targets
  COMPREPLY=()

  # Calling `mach-completion` with -h/--help would result in the
  # help text being used as the completion targets.
  if [[ $COMP_LINE == *"-h"* || $COMP_LINE == *"--help"* ]]; then
    return 0
  fi

  # Load the list of targets
  targets=`"${COMP_WORDS[0]}" mach-completion ${COMP_LINE}`
  cur="${COMP_WORDS[COMP_CWORD]}"
  COMPREPLY=( $(compgen -W "$targets" -- ${cur}) )
  return 0
}
complete -o default -F _mach mach
