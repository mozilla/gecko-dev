#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# This file downloads the dependencies needed to build JPEG XL into third_party.
# These dependencies are normally pulled by git.

set -eu

SELF=$(realpath "$0")
MYDIR=$(dirname "${SELF}")

# Git revisions we use for the given submodules. Update these whenever you
# update a git submodule.
TESTDATA="873045a9c42ed60721756e26e2a6b32e17415205"
THIRD_PARTY_BROTLI="36533a866ed1ca4b75cf049f4521e4ec5fe24727"
THIRD_PARTY_GOOGLETEST="58d77fa8070e8cec2dc1ed015d66b454c8d78850"
THIRD_PARTY_HIGHWAY="457c891775a7397bdb0376bb1031e6e027af1c48"
THIRD_PARTY_SKCMS="42030a771244ba67f86b1c1c76a6493f873c5f91"
THIRD_PARTY_SJPEG="e5ab13008bb214deb66d5f3e17ca2f8dbff150bf"
THIRD_PARTY_ZLIB="51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf" # v1.3.1
THIRD_PARTY_LIBPNG="f135775ad4e5d4408d2e12ffcc71bb36e6b48551" # v1.6.40
THIRD_PARTY_LIBJPEG_TURBO="8ecba3647edb6dd940463fedf38ca33a8e2a73d1" # 2.1.5.1

# Download the target revision from GitHub.
download_github() {
  local path="$1"
  local project="$2"

  local varname=`echo "$path" | tr '[:lower:]' '[:upper:]'`
  varname="${varname//[\/-]/_}"
  local sha
  eval "sha=\${${varname}}"

  local down_dir="${MYDIR}/downloads"
  local local_fn="${down_dir}/${sha}.tar.gz"
  if [[ -e "${local_fn}" && -d "${MYDIR}/${path}" ]]; then
    echo "${path} already up to date." >&2
    return 0
  fi

  local url
  local strip_components=0
  if [[ "${project:0:4}" == "http" ]]; then
    # "project" is a googlesource.com base url.
    url="${project}${sha}.tar.gz"
  else
    # GitHub files have a top-level directory
    strip_components=1
    url="https://github.com/${project}/tarball/${sha}"
  fi

  echo "Downloading ${path} version ${sha}..." >&2
  mkdir -p "${down_dir}"
  curl -L --show-error -o "${local_fn}.tmp" "${url}"
  mkdir -p "${MYDIR}/${path}"
  tar -zxf "${local_fn}.tmp" -C "${MYDIR}/${path}" \
    --strip-components="${strip_components}"
  mv "${local_fn}.tmp" "${local_fn}"
}

is_git_repository() {
    local dir="$1"
    local toplevel=$(git rev-parse --show-toplevel)

    [[ "${dir}" == "${toplevel}" ]]
}


main() {
  if is_git_repository "${MYDIR}"; then
    cat >&2 <<EOF
Current directory is a git repository, downloading dependencies via git:

  git submodule update --init --recursive

EOF
    git -C "${MYDIR}" submodule update --init --recursive --depth 1 --recommend-shallow
    return 0
  fi

  # Sources downloaded from a tarball.
  download_github testdata libjxl/testdata
  download_github third_party/brotli google/brotli
  download_github third_party/googletest google/googletest
  download_github third_party/highway google/highway
  download_github third_party/sjpeg webmproject/sjpeg
  download_github third_party/skcms \
    "https://skia.googlesource.com/skcms/+archive/"
  download_github third_party/zlib madler/zlib
  download_github third_party/libpng glennrp/libpng
  download_github third_party/libjpeg-turbo libjpeg-turbo/libjpeg-turbo
  echo "Done."
}

main "$@"
