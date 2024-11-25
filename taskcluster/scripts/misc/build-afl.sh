#!/bin/sh

set -e -x

artifact="$(basename "$TOOLCHAIN_ARTIFACT")"
dir="${artifact%.tar.*}"
scripts="$(realpath "${0%/*}")"

# these lists are copied from AFLplusplus/GNUmakefile
PROGS="afl-fuzz afl-showmap afl-tmin afl-gotcpu afl-analyze"
SH_PROGS="afl-plot afl-cmin afl-cmin.bash afl-whatsup afl-addseeds afl-system-config afl-persistent-config"

cd "$MOZ_FETCHES_DIR/AFLplusplus"
patch -p1 -i "$scripts/afl-nyx.patch"

make -f GNUmakefile $PROGS \
    CC="$MOZ_FETCHES_DIR/clang/bin/clang" \
    CFLAGS="--sysroot $MOZ_FETCHES_DIR/sysroot" \
    CODE_COVERAGE=1 \
    NO_PYTHON=1 \
    PREFIX=/
mkdir -p "$dir/bin"
install -m 755 $PROGS $SH_PROGS "$dir/bin"

make -f GNUmakefile.llvm install \
    CODE_COVERAGE=1 \
    CPPFLAGS="--sysroot $MOZ_FETCHES_DIR/sysroot" \
    DESTDIR="$dir" \
    LLVM_CONFIG="$MOZ_FETCHES_DIR/clang/bin/llvm-config" \
    PREFIX=/
rm -rf "$dir/share"

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
