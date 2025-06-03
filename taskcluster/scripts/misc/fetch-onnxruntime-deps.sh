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

