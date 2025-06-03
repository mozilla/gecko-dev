#!/bin/bash

# === Prerequisites ===
# Requires a Linux or macOS system with:
# - Git installed (Mac: `xcode-select --install`, Ubuntu: `sudo apt install git`)
# - Node.js >= v23.9.0 (see https://docs.npmjs.com/downloading-and-installing-node-js-and-npm)
# - Docker >= 4.37 (Mac: https://docs.docker.com/desktop/setup/install/mac-install/,
#                   Linux: https://docs.docker.com/desktop/setup/install/linux/)
# - Docker must be running

# === Output ===
# - Generates `wllama.wasm`, `wllama-module.mjs` and `wllama-module-dev.mjs` in the script's directory.
# - Copy `wllama-module.mjs` and `wllama-module-dev.mjs` to `toolkit/components/ml/vendor`
# - Upload `wllama.wasm` to Remote Settings to complete the vendoring update.
# - Update `wllama.wasm` task in taskcluster/kinds/fetch/onnxruntime-web-fetch.yml with updated wasm location

curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/master/install.sh | bash
. ~/.nvm/nvm.sh
nvm install 23 # Ensure Node.js v23 is installed

TMP_DIR=$(mktemp -d)  # Create a temporary working directory

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" # Get script's directory

set -e  # Exit immediately on error

cleanup() {
    popd || true  # Restore original directory, ignore errors
    rm -rf "${TMP_DIR}" # Remove temporary directory
}

trap cleanup EXIT  # Ensure cleanup runs on script exit

pushd "${TMP_DIR}"  # Switch to temp dir, saving previous location

# Clone Wllama repository (v2.3.1) with submodules
git clone --branch 2.3.1 --single-branch --recurse-submodules https://github.com/ngxson/wllama
cd wllama

# Apply local patch
git apply --recount --ignore-whitespace ${SCRIPT_DIR}/diff.patch


# === Clean up existing build artifacts ===
rm -rf dist wasm # Remove cached build files
docker compose rm -fsv llamacpp-wasm-builder 2>/dev/null  || true # Remove docker build service
docker rm -fv lamacpp-wasm-builder 2>/dev/null || true # Ensure container is removed
docker rmi $(docker inspect --format='{{.Image}}' lamacpp-wasm-builder) 2>/dev/null || true # Remove its image

# Install dependencies and build
npm i # Install required npm modules
npm run build:wasm # Compile WebAssembly
npm run build:worker # Build the worker script

# Generate minified production build
npx --yes esbuild index.ts --bundle --minify --format=esm --platform=browser --outfile=dist/wllama-module.mjs

# Generate dev build
npx --yes esbuild index.ts --bundle --format=esm --platform=browser --outfile=dist/wllama-module-dev.mjs

# Copy final artifacts to script directory
cp dist/wllama-module.mjs "${SCRIPT_DIR}"
cp dist/wllama-module-dev.mjs "${SCRIPT_DIR}"
cp src/multi-thread/wllama.wasm "${SCRIPT_DIR}"