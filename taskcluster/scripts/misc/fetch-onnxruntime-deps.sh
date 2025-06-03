#!/bin/sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script downloads onnxruntime dependencies and bundles them in a single archive.
# It avoids downloading them at build time. Not all dependencies are actually
# used during build, they are selected at configuration time.

set -e
set -x

# script parameters
onnx_git=$1
onnx_rev=$2
tardir=onnxruntime-deps

# script dependencies
git=`which git`
curl=`which curl`

# parse onnxruntime dependencies from original depo
currdir=`pwd`
workdir=`mktemp -d`
cd $workdir
curl -LO "$onnx_git/archive/$onnx_rev.tar.gz"
tar xf "$onnx_rev.tar.gz"
cd onnxruntime-$onnx_rev

# Update source for eigen3, see https://github.com/microsoft/onnxruntime/pull/24884
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

mkdir $tardir
cd $tardir

# actual download
grep -v '^#' ../cmake/deps.txt | cut -d ';' -f2 | while read url ; do $curl -LO $url ; done

# packit
cd ..
tar caf $tardir.tar.zst $tardir
mkdir -p $UPLOAD_DIR
mv $tardir.tar.zst $UPLOAD_DIR

# cleanup
cd  $currdir
rm -rf $workdir

