#!/bin/bash
set -x -e -v

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}
workspace=$HOME/workspace

cd $MOZ_FETCHES_DIR/$project

gcc_major=10
export CFLAGS=--sysroot=$MOZ_FETCHES_DIR/sysroot
export CXXFLAGS"=--sysroot=$MOZ_FETCHES_DIR/sysroot  -isystem $MOZ_FETCHES_DIR/sysroot/usr/include/c++/$gcc_major -isystem $MOZ_FETCHES_DIR/sysroot/usr/include/x86_64-linux-gnu/c++/$gcc_major"
export LDFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot -L$MOZ_FETCHES_DIR/sysroot/lib/x86_64-linux-gnu -L$MOZ_FETCHES_DIR/sysroot/usr/lib/x86_64-linux-gnu -L$MOZ_FETCHES_DIR/sysroot/usr/lib/gcc/x86_64-linux-gnu/$gcc_major"
export CC=$MOZ_FETCHES_DIR/gcc/bin/gcc
export CXX=$MOZ_FETCHES_DIR/gcc/bin/g++

# The glibc in our sysroot doesn't have `sys/random.h`/`getrandom`.
sed -i '/HAVE_SYS_RANDOM_H/d;/HAVE_GETRANDOM/d' deps/cares/config/linux/ares_config.h

# --partly-static allows the resulting binary to run on Ubuntu 18.04 (which has libstdc++ 8)
./configure --verbose --prefix=/ --partly-static
make -j$(nproc) install DESTDIR=$workspace/$project

tar -C $workspace -acvf $artifact $project
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR
