#!/bin/bash
set -x -e -v

TARGET=$1
. $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

cd $MOZ_FETCHES_DIR/make

chmod +w src/config.h.W32
sed "/#define BATCH_MODE_ONLY_SHELL/s/\/\*\(.*\)\*\//\1/" src/config.h.W32 > src/config.h
make -f Basic.mk \
  MAKE_HOST=Windows32 \
  MKDIR.cmd='mkdir -p $1' \
  RM.cmd='rm -f $1' \
  CP.cmd='cp $1 $2' \
  msvc_CC="$MOZ_FETCHES_DIR/clang/bin/clang-cl --target=$TARGET -Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml" \
  msvc_LD=$MOZ_FETCHES_DIR/clang/bin/lld-link

mkdir mozmake
cp WinRel/gnumake.exe mozmake/mozmake.exe

tar -acvf mozmake.tar.zst mozmake
mkdir -p $UPLOAD_DIR
cp mozmake.tar.zst $UPLOAD_DIR
