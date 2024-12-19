#!/bin/bash
set -x -e -v

# This script is for building 7-zip for Linux.
PROJECT=7zz

cd ${MOZ_FETCHES_DIR}/${PROJECT}

# Replace CR/LF line endings with Unix LF endings
find . -name "*.mak" -exec sed -i 's/\r$//' {} \;
pushd CPP/7zip/Bundles/Alone2
make -f ../../cmpl_gcc.mak
popd

mkdir ${PROJECT}
mv CPP/7zip/Bundles/Alone2/b/g/7zz ${PROJECT}/7zz
tar -acf ${PROJECT}.tar.zst ${PROJECT}

mkdir -p $UPLOAD_DIR
mv ${PROJECT}.tar.zst $UPLOAD_DIR
