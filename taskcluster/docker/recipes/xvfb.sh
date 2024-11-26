#! /bin/bash -x

set -x

fail() {
    echo # make sure error message is on a new line
    echo "[xvfb.sh:error]" "${@}"
    exit 1
}

cleanup_xvfb() {
    # When you call this script with START_VNC or TASKCLUSTER_INTERACTIVE
    # we make sure we do not kill xvfb so you do not lose your connection
    local xvfb_pid=`pidof Xvfb`
    local vnc=${START_VNC:-false}
    local interactive=${TASKCLUSTER_INTERACTIVE:-false}
    if [ -n "$xvfb_pid" ] && [[ $vnc == false ]] && [[ $interactive == false ]] ; then
        kill $xvfb_pid || true
    fi
}

start_xvfb() {
    mkdir -p ~/artifacts/xvfb
    # Add a handler for SIGUSR1
    trap : SIGUSR1
    # Start Xvfb with SIGUSR1 set to SIG_IGN; it will then signal its parent when it's ready to accept connections
    (trap '' SIGUSR1; exec Xvfb :$2 -nolisten tcp -noreset -screen 0 $1 > ~/artifacts/xvfb/xvfb.log 2>&1) &
    xvfb_pid=$!
    # Wait for SIGUSR1 (or Xvfb exit in case of error)
    set +e
    wait $xvfb_pid
    wait_result=$?
    if [ $wait_result -ne $((128 + $(kill -l SIGUSR1) )) ]; then
        fail "Xvfb failed to start" "$(cat ~/artifacts/xvfb/xvfb.log >&2)"
    fi
    set -e

    export DISPLAY=:$2
}
