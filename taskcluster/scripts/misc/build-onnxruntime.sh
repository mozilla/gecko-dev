#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -x -e -v

# This script is for building the onnx runtime using upstream build system.

target_platform="$1"
target_arch="$2"

###
# Gather build dependencies from fetches

if test -d "$MOZ_FETCHES_DIR/cmake"; then
    export PATH="$(cd $MOZ_FETCHES_DIR/cmake && pwd)/bin:${PATH}"
fi

onnxruntime_depdir="$MOZ_FETCHES_DIR/onnxruntime-deps"

export PATH="$(cd $MOZ_FETCHES_DIR/ninja && pwd)/bin:${PATH}"
export PATH="$(cd $MOZ_FETCHES_DIR/clang && pwd)/bin:${PATH}"

export CC=clang
export CXX=clang++

###
# Extra setup per platform
case ${target_platform} in
    Darwin)
        # Use taskcluster clang instead of host compiler on OSX
        osx_sysroot=`cd ${MOZ_FETCHES_DIR}/MacOSX*.sdk; pwd`
        extra_args="--cmake_extra_defines CMAKE_OSX_SYSROOT=${osx_sysroot} --osx_arch $target_arch"
        prefix=lib
        extension=dylib
        ;;
    Linux)
        prefix=lib
        extension=so
        ;;
    Android)
        extra_args="--android --android_ndk_path=$MOZ_FETCHES_DIR/android-ndk --android_sdk_path=$MOZ_FETCHES_DIR/android-sdk-linux --android_abi=$target_arch"
        prefix=lib
        extension=so
        ;;
    Windows)
        # Still use visual studio there, compilation through clang-cl is not
        # supported upstream.
        case $target_arch in
            x86)
                extra_args="--x86"
                ;;
        esac
        . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
        sed -i -e 's/ProgramDatabase//' "$MOZ_FETCHES_DIR/onnxruntime/tools/ci_build/build.py"
        export CC=cl.exe
        export CXX=cl.exe
        prefix=
        extension=dll
        ;;
esac

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
onnx_folder=${artifact%.tar.*}

cd "$MOZ_FETCHES_DIR/onnxruntime"

###
# Various patches

# Update checksum for eigen3, see https://github.com/microsoft/onnxruntime/pull/24884
patch -p1 << EOF
diff --git a/cmake/deps.txt b/cmake/deps.txt
index 728241840f723..6e045f6dcdc9d 100644
--- a/cmake/deps.txt
+++ b/cmake/deps.txt
@@ -22,7 +22,9 @@ dlpack;https://github.com/dmlc/dlpack/archive/5c210da409e7f1e51ddf445134a4376fdb
 # it contains changes on top of 3.4.0 which are required to fix build issues.
 # Until the 3.4.1 release this is the best option we have.
 # Issue link: https://gitlab.com/libeigen/eigen/-/issues/2744
-eigen;https://gitlab.com/libeigen/eigen/-/archive/1d8b82b0740839c0de7f1242a3585e3390ff5f33/eigen-1d8b82b0740839c0de7f1242a3585e3390ff5f33.zip;5ea4d05e62d7f954a46b3213f9b2535bdd866803
+# Moved to github mirror to avoid gitlab issues.
+# Issue link: https://github.com/bazelbuild/bazel-central-registry/issues/4355
+eigen;https://github.com/eigen-mirror/eigen/archive/1d8b82b0740839c0de7f1242a3585e3390ff5f33/eigen-1d8b82b0740839c0de7f1242a3585e3390ff5f33.zip;05b19b49e6fbb91246be711d801160528c135e34
 flatbuffers;https://github.com/google/flatbuffers/archive/refs/tags/v23.5.26.zip;59422c3b5e573dd192fead2834d25951f1c1670c
 fp16;https://github.com/Maratyszcza/FP16/archive/0a92994d729ff76a58f692d3028ca1b64b145d91.zip;b985f6985a05a1c03ff1bb71190f66d8f98a1494
EOF

# Make sure we use dependencies from onnxruntime-deps and avoid re-downloading
# them.
sed -i -e "s,;.*/,;$onnxruntime_depdir/,g"  cmake/deps.txt

# Apply local patches
find $GECKO_PATH/taskcluster/scripts/misc/onnxruntime.patches -type f -name '*.patch' -print0 | sort -z | while read -d '' patch ; do patch -p1 < $patch ; done

###
# Configure and build
onnx_builddir=_build

mkdir $onnx_builddir

build_type=MinSizeRel

python3 tools/ci_build/build.py \
    --update \
    --parallel \
    --enable_lto \
    --disable_rtti \
    --cmake_generator Ninja \
    --build_dir $onnx_builddir \
    --build --build_shared_lib \
    --skip_submodule_sync --use_lock_free_queue --skip_tests --config $build_type --compile_no_warning_as_error \
    --cmake_extra_defines onnxruntime_BUILD_UNIT_TESTS=OFF\
    --cmake_extra_defines PYTHON_EXECUTABLE=$(which python3)\
    --cmake_extra_defines ONNX_USE_LITE_PROTO=ON\
    --disable_exceptions \
    --cmake_extra_defines CMAKE_CXX_FLAGS=-fno-exceptions\ -DORT_NO_EXCEPTIONS\ -DONNX_NO_EXCEPTIONS\ -DMLAS_NO_EXCEPTION\
    ${extra_args}

###
# Pack the result and upload.
mkdir $onnx_folder
cp $onnx_builddir/$build_type/${prefix}onnxruntime.${extension} $onnx_folder/

ls -la "$onnx_folder"

find "$onnx_folder" -type f -exec llvm-strip -x {} \;  || true

ls -la "$onnx_folder"
mkdir -p $UPLOAD_DIR

export ZSTD_CLEVEL=19

case ${target_platform} in
    Windows)
        tar -a -c -f "$UPLOAD_DIR/$artifact" --force-local "$onnx_folder"
        ;;
    *)
        tar acf "$UPLOAD_DIR/$artifact" "$onnx_folder"
        ;;
esac
ls -la "$UPLOAD_DIR/$artifact"
