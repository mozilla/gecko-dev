#!/usr/bin/env bash

. $(dirname "$0")/tools.sh

set -e

test -n "${VCS_PATH}"

# builds write to the source dir (and its parent), so move the source trees to
# our workspace from the (cached) checkout dir
cp -a "${VCS_PATH}/nspr" "${VCS_PATH}/nss" .

pushd nspr
hg revert --all
if [ -f "../nss/nspr.patch" ] && [ "$ALLOW_NSPR_PATCH" = "1" ]; then
  patch -p1 < ../nss/nspr.patch
fi
popd

# Dependencies
# For MacOS we have hardware in the CI which doesn't allow us to deploy VMs.
# The setup is hardcoded and can't be changed easily.
# This part is a helper We install dependencies manually to help.
if [ "$(uname)" = "Darwin" ]; then
  python3 -m pip install --user gyp-next
  python3 -m pip install --user ninja
  export PATH="$(python3 -m site --user-base)/bin:${PATH}"
fi

# Build.
nss/build.sh -g -v --enable-libpkix -Denable_draft_hpke=1 "$@"

# Package.
if [ "$(uname)" = "Darwin" ]; then
  mkdir -p public
  tar cvfjh public/dist.tar.bz2 dist
else
  if [ "$(uname)" = Linux ]; then
    ln -s /builds/worker/artifacts artifacts
  else
    mkdir artifacts
  fi
  tar cvfjh artifacts/dist.tar.bz2 dist
fi
