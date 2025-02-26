#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

set -e

test -n "${VCS_PATH}"

# Generate certificates.
NSS_TESTS=cert NSS_CYCLES="standard pkix sharedb" $(dirname $0)/run_tests.sh

# Reset test counter so that test runs pick up our certificates.
echo 1 > tests_results/security/localhost

# Package.
if [ $(uname) = Linux ]; then
    artifacts=/builds/worker/artifacts
else
    mkdir public
    artifacts=public
fi
tar cvfjh ${artifacts}/dist.tar.bz2 dist tests_results
