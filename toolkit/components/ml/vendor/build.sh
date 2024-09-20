# !/bin/bash
set -xe

# grabbing and patching transformers.js for gecko
rm -rf tmp
mkdir tmp
pushd tmp
# pin to a specific commit on the v3 branch
git clone --branch v3 https://github.com/xenova/transformers.js
cd transformers.js
git checkout 682c7d0588e1ea65090adb5be50877b8d8bc0968
git apply ../../gecko.patch
npm install
npm install typescript@latest
npm install @types/node@latest
npm run build
cp dist/transformers.js ../../transformers-dev.js
cp dist/transformers.min.js ../../transformers.js
popd
rm -rf tmp

# grabbing and patching onnxruntime-web for gecko.

# fetch the tarball URL from npm
TARBALL_URL=$(npm view onnxruntime-web@1.20.0-dev.20240827-1d059b8702 dist.tarball)
wget "${TARBALL_URL}" -O dist.tgz

# grab the two files we need
tar -xzf dist.tgz
rm dist.tgz
cp package/dist/ort.mjs ort-dev.mjs
cp package/dist/ort.min.mjs ort.mjs
cp package/dist/ort.webgpu.mjs ort.webgpu-dev.mjs
cp package/dist/ort.webgpu.min.mjs ort.webgpu.mjs
cp package/dist/ort-wasm-simd-threaded.jsep.mjs ort-wasm-simd-threaded.jsep.mjs
cp package/dist/ort-wasm-simd-threaded.mjs ort-wasm-simd-threaded.mjs

rm -rf package

# remove the last line of ort-dev.js and ort.webgpu-dev.mjs (map)
sed -i '' '$d' ort-dev.mjs
sed -i '' '$d' ort.webgpu-dev.mjs
