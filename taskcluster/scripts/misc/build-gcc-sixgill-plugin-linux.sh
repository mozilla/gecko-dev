#!/bin/bash

set -e
set -x

# This script is for building the sixgill GCC plugin for Linux. It relies on
# the gcc checkout because it needs to recompile gmp and the gcc build script
# determines the version of gmp to download.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
build_dir=$HOME_DIR/src/build
data_dir=$HOME_DIR/src/build/unix/build-gcc

# Download and unpack upstream toolchain artifacts (ie, the gcc binary).
. $(dirname $0)/tooltool-download.sh

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.28.1
binutils_ext=xz
sixgill_rev=bc0ef9258470
sixgill_repo=https://hg.mozilla.org/users/sfink_mozilla.com/sixgill

. $data_dir/build-gcc.sh

pushd $root_dir/gcc-$gcc_version
ln -sf ../binutils-2.28.1 binutils
ln -sf ../gmp-5.1.3 gmp
ln -sf ../isl-0.15 isl
ln -sf ../mpc-0.8.2 mpc
ln -sf ../mpfr-3.1.5 mpfr
popd

export TMPDIR=${TMPDIR:-/tmp/}
export gcc_bindir=$root_dir/src/gcc/bin
export gmp_prefix=/tools/gmp
export gmp_dir=$root_dir$gmp_prefix

prepare_sixgill() {(
    cd $root_dir
    hg clone -r $sixgill_rev $sixgill_repo || ( cd sixgill && hg update -r $sixgill_rev )
)}

build_gmp() {
    if ! [ -x $gcc_bindir/gcc ]; then
        echo "GCC not found in $gcc_bindir/gcc" >&2
        exit 1
    fi

    # The sixgill plugin uses some gmp symbols, including some not exported by
    # cc1/cc1plus. So link the plugin statically to libgmp. Except that the
    # default static build does not have -fPIC, and will result in a relocation
    # error, so build our own. This requires the gcc and related source to be
    # in $root_dir/gcc-$gcc_version.

    mkdir $root_dir/gmp-objdir || true
    (
        cd $root_dir/gmp-objdir
        $root_dir/gcc-$gcc_version/gmp/configure --disable-shared --with-pic --prefix=$gmp_prefix
        make -j8
        make install DESTDIR=$root_dir
    )
}

build_sixgill() {(
    cd $root_dir/sixgill
    export CC=$gcc_bindir/gcc
    export CXX=$gcc_bindir/g++
    export PATH="$gcc_bindir:$PATH"
    export LD_LIBRARY_PATH="${gcc_bindir%/bin}/lib64"
    export TARGET_CC=$CC
    export CPPFLAGS=-I$gmp_dir/include
    export EXTRA_LDFLAGS=-L$gmp_dir/lib
    export HOST_CFLAGS=$CPPFLAGS

    ./release.sh --build-and-package --with-gmp=$gmp_dir
    tarball=$(ls -td *-sixgill | head -1)/sixgill.tar.xz
    cp $tarball $root_dir/sixgill.tar.xz
)}

prepare_sixgill
build_gmp
build_sixgill

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/sixgill.tar.* $UPLOAD_DIR
