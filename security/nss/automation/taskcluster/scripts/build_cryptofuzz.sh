#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Do differential fuzzing with Botan (and not OpenSSL) since NSS has
# symbol collisions with OpenSSL and therefore they can't be used together
# in Cryptofuzz.
export CRYPTOFUZZ_VERSION="687d3064c5cef2b0fe1f30824065a2f2c9c0bbd8"
export BOTAN_VERSION="3.6.1"

git clone -q https://github.com/MozillaSecurity/cryptofuzz.git
git -C cryptofuzz checkout "$CRYPTOFUZZ_VERSION"

git clone -q https://github.com/randombit/botan.git
git -C botan checkout "$BOTAN_VERSION"

export CC="clang"
export CXX="clang++"

export CFLAGS="-fsanitize=address,fuzzer-no-link -O2 -g"
export CXXFLAGS="-fsanitize=address,fuzzer-no-link -O2 -g"

if [ "$1" = "--i386" ]; then
    # Make sure everything is compiled and linked with 32-bit.
    export CFLAGS="$CFLAGS -m32"
    export CXXFLAGS="$CXXFLAGS -m32"

    export LD_FLAGS="$LD_FLAGS -m32"
    export LINK_FLAGS="$LINK_FLAGS -m32"

    # Some static libraries aren't built on 32-bit systems, but still assumed
    # to exist by Cryptofuzz.
    sed -i "/libhw-acc-crypto-avx.a/d" cryptofuzz/modules/nss/Makefile
    sed -i "/libhw-acc-crypto-avx2.a/d" cryptofuzz/modules/nss/Makefile
else
    # UBSan is only enabled for 64-bit builds of NSS.
    export CFLAGS="$CFLAGS -fsanitize=undefined"
    export CXXFLAGS="$CXXFLAGS -fsanitize=undefined"
fi

# Build Botan.
pushd botan
if [ "$1" = "--i386" ]; then
    ./configure.py --cpu=x86_32 \
                   --cc-bin=$CXX \
                   --cc-abi-flags="$CXXFLAGS" \
                   --disable-shared \
                   --disable-modules=locking_allocator \
                   --build-targets=static \
                   --without-documentation
else
    ./configure.py --cc-bin=$CXX \
                   --cc-abi-flags="$CXXFLAGS" \
                   --disable-shared \
                   --disable-modules=locking_allocator \
                   --build-targets=static \
                   --without-documentation
fi
make -j"$(nproc)"
popd

# Generate Cryptofuzz header.
pushd cryptofuzz
./gen_repository.py
popd

# Build Botan module.
export CXXFLAGS="$CXXFLAGS -DCRYPTOFUZZ_BOTAN"
export LIBBOTAN_A_PATH="$(realpath botan/libbotan-3.a)"
export BOTAN_INCLUDE_PATH="$(realpath botan/build/include)"

pushd cryptofuzz/modules/botan
make -j"$(nproc)"
popd

# Build NSS module.
export NSS_NSPR_PATH="$PWD"
export CXXFLAGS="$CXXFLAGS -I $NSS_NSPR_PATH/dist/public/nss -I $NSS_NSPR_PATH/dist/Debug/include/nspr -DCRYPTOFUZZ_NSS -DCRYPTOFUZZ_NO_OPENSSL"
export LINK_FLAGS="$LINK_FLAGS -lsqlite3"

# The library lies somewhere else than what is expected by Cryptofuzz.
sed -i "s/nspr\/Debug\/pr\/src/dist\/Debug\/lib/" cryptofuzz/modules/nss/Makefile

pushd cryptofuzz/modules/nss
make -j"$(nproc)"
popd

# Build Cryptofuzz.
export LIBFUZZER_LINK="-fsanitize=fuzzer"

pushd cryptofuzz
make -j"$(nproc)"
popd

# Package
mkdir -p artifacts
tar cvfjh artifacts/cryptofuzz.tar.bz2 cryptofuzz
