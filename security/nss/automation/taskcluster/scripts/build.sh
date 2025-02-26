#!/usr/bin/env bash

. $(dirname "$0")/tools.sh

set -e

test -v VCS_PATH

# builds write to the source dir (and its parent), so move the source trees to
# our workspace from the (cached) checkout dir
cp -a "${VCS_PATH}/nss" "${VCS_PATH}/nspr" .

if [ -n "$NSS_BUILD_MODULAR" ]; then
    ln -sf /builds/worker/artifacts artifacts
    $(dirname "$0")/build_nspr.sh || exit $?
    $(dirname "$0")/build_util.sh || exit $?
    $(dirname "$0")/build_softoken.sh || exit $?
    $(dirname "$0")/build_nss.sh || exit $?
    exit
fi

pushd nspr
hg revert --all
if [[ -f ../nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  patch -p1 < ../nss/nspr.patch
fi
popd

# Build.
make -C nss nss_build_all

# Package.
if [ `uname` = Linux ]; then
    artifacts=/builds/worker/artifacts
else
    mkdir artifacts
    artifacts=artifacts
fi
tar cvfjh ${artifacts}/dist.tar.bz2 dist
