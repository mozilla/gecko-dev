# !/bin/bash
set -xe

TRANSFORMERS_VERSION=3.4.0
ONNXRUNTIME_VERSION=1.22.0-dev.20250306-ccf8fdd9ea

# grabbing and patching transformers.js for gecko
rm -rf tmp
mkdir tmp
pushd tmp

git clone --branch $TRANSFORMERS_VERSION --depth 1 https://github.com/huggingface/transformers.js
cd transformers.js
git apply ../../gecko.patch
popd

# building and running docker
docker build -t transformers-build .
docker run --rm -it -v $(pwd)/tmp/transformers.js:/tmp/transformers.js transformers-build

cp $(pwd)/tmp/transformers.js/dist/transformers.dev.js ../transformers-dev.js
cp $(pwd)/tmp/transformers.js/dist/transformers.min.js ../transformers.js

rm -rf tmp

# grabbing and patching onnxruntime-web for gecko.

# fetch the tarball URL from npm
TARBALL_URL=$(npm view onnxruntime-web@$ONNXRUNTIME_VERSION dist.tarball)
wget "${TARBALL_URL}" -O dist.tgz

# grab the files we need
tar -xzf dist.tgz
rm dist.tgz
cp package/dist/ort.mjs ../ort-dev.mjs
cp package/dist/ort.min.mjs ../ort.mjs
cp package/dist/ort.webgpu.mjs ../ort.webgpu-dev.mjs
cp package/dist/ort.webgpu.min.mjs ../ort.webgpu.mjs
cp package/dist/ort-wasm-simd-threaded.jsep.mjs ../ort-wasm-simd-threaded.jsep.mjs
cp package/dist/ort-wasm-simd-threaded.jsep.wasm ../ort-wasm-simd-threaded.jsep.wasm

rm -rf package

# remove the last line of ort-dev.js and ort.webgpu-dev.mjs (map)
sed -i '' '$d' ../ort-dev.mjs
sed -i '' '$d' ../ort.webgpu-dev.mjs
