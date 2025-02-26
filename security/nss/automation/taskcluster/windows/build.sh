#!/usr/bin/env bash

set -v -e -x

test -v VCS_PATH

# builds write to the source dir (and its parent), so move the source trees to
# our workspace from the (cached) checkout dir
cp -a "${VCS_PATH}/nss" "${VCS_PATH}/nspr" .

if [[ "$USE_64" == 1 ]]; then
    m=x64
else
    m=x86
fi
source "$(dirname "$0")/setup.sh"

pushd nspr
hg revert --all
if [[ -f ../nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  cat ../nss/nspr.patch | patch -p1
fi
popd

# Build.
mozmake -C nss nss_build_all

# Package.
7z a public/build/dist.7z dist
