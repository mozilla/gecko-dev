#!/bin/bash

set -eux

# ensure the prove testing tool is available
echo "::group::ensure build/test prerequisites"
if ! command -v prove; then
  if grep -m 1 alpine /etc/os-release; then
    apk add perl-utils
  else
    echo "prove (perl) testing tool unavailable"
    exit 1
  fi
fi
echo "::endgroup::"

# build the requested version of libyaml locally
echo "::group::fetch libyaml ${LIBYAML_REF}"
git config --global advice.detachedHead false
git clone --branch "$LIBYAML_REF" "$LIBYAML_REPO" libyaml
pushd libyaml
git reset --hard "$LIBYAML_REF"
echo "::endgroup::"

echo "::group::autoconf libyaml w/ static only"
./bootstrap
# build only a static library- reduces our reliance on auditwheel/delocate magic
./configure --disable-dependency-tracking --with-pic --enable-shared=no
echo "::endgroup::"

echo "::group::build libyaml"
make
echo "::endgroup::"

echo "::group::test built libyaml"
make test-all
echo "::endgroup::"
popd
