#!/bin/bash
set -e

# This script is for building a MinGW GCC (and headers) to be used on Linux to compile for Windows.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=6.4.0
gcc_target=$1
gcc_ext=xz
binutils_version=2.27
binutils_ext=bz2
binutils_configure_flags="--target=$gcc_target"
mingw_version=70860d945e6be713af352ee62820bccb653589c2

pushd $root_dir/gcc-$gcc_version
ln -sf ../gmp-5.1.3 gmp
ln -sf ../isl-0.15 isl
ln -sf ../mpc-0.8.2 mpc
ln -sf ../mpfr-3.1.5 mpfr
popd

prepare_mingw
build_binutils
build_gcc_and_mingw

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $root_dir/mingw32.tar.* $UPLOAD_DIR
