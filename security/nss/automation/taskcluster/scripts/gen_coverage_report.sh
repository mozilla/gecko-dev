#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

cp -a ${VCS_PATH}/nss ${VCS_PATH}/nspr .

pushd nspr
hg revert --all
if [[ -f ../nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  cat ../nss/nspr.patch | patch -p1
fi
popd

out=/builds/worker/artifacts
mkdir -p $out

# Generate coverage report.
cd nss && ./mach coverage --outdir=$out ssl_gtests
