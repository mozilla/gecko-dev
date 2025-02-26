#!/usr/bin/env bash

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    # Note: this mangles arguments, better to avoid running scripts as root.
    exec su worker -c "$0 $*"
fi

set -e -x -v

# Get the HACL* source, containing a snapshot of the C code, extracted on the
# HACL CI.
git clone -q "https://github.com/hacl-star/hacl-star" ~/hacl-star
git -C ~/hacl-star checkout -q 0f136f28935822579c244f287e1d2a1908a7e552

# Format the C snapshot.
cd ~/hacl-star/dist/mozilla
cp ${VCS_PATH}/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+
cd ~/hacl-star/dist/karamel
cp ${VCS_PATH}/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+
cd ~/hacl-star/dist/gcc-compatible
cp ${VCS_PATH}/nss/.clang-format .
find . -type f -name '*.[ch]' -exec clang-format -i {} \+

cd ~/hacl-star
patches=(${VCS_PATH}/nss/automation/taskcluster/scripts/patches/*.patch)
for f in "${patches[@]}"; do
    git apply "$f"
done

# These diff commands will return 1 if there are differences and stop the script.

# We have two checks in the script. 
# The first one only checks the files in the verified/internal folder; the second one does for all the rest
# It was implemented like this due to not uniqueness of the names in the verified folders
# For instance, the files Hacl_Chacha20.h are present in both directories, but the content differs.

# TODO(Bug 1899443): remove these exceptions
files=($(find ${VCS_PATH}/nss/lib/freebl/verified/internal -type f -name '*.[ch]' -not -path "*/freebl/verified/internal/libcrux*"))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/mozilla/internal/ -type f -name $file_name))
    if [ $file_name == "Hacl_Ed25519.h" \
        -o $file_name == "Hacl_Ed25519_PrecompTable.h" ]
    then
        continue
    fi
    diff -u $hacl_file $f
done

files=($(find ${VCS_PATH}/nss/lib/freebl/verified/ -type f -name '*.[ch]' -not -path "*/freebl/verified/internal/*" -not -path "*/freebl/verified/config.h" -not -path "*/freebl/verified/libcrux*"))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/mozilla/ ~/hacl-star/dist/karamel/ -type f -name $file_name -not -path "*/hacl-star/dist/mozilla/internal/*"))
    if [ $file_name == "Hacl_P384.c"  \
        -o $file_name == "Hacl_P384.h" \
        -o $file_name == "Hacl_P521.c" \
        -o $file_name == "Hacl_P521.h" \
        -o $file_name == "eurydice_glue.h" \
        -o $file_name == "target.h" ]
    then
        continue
    fi

    if [ $file_name == "Hacl_Ed25519.h"  \
        -o $file_name == "Hacl_Ed25519.c" ]
    then
        continue
    fi
    diff -u $hacl_file $f
done

# Here we process the code that's not located in /hacl-star/dist/mozilla/ but
# /hacl-star/dist/gcc-compatible. 

files=($(find ${VCS_PATH}/nss/lib/freebl/verified/internal -type f -name '*.[ch]'))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/gcc-compatible/internal/ -type f -name $file_name))
    if [ $file_name != "Hacl_Ed25519.h" \
        -a $file_name != "Hacl_Ed25519_PrecompTable.h" ]
    then
        continue
    fi  
    diff -u $hacl_file $f
done

files=($(find ${VCS_PATH}/nss/lib/freebl/verified/ -type f -name '*.[ch]' -not -path "*/freebl/verified/internal/*"))
for f in "${files[@]}"; do
    file_name=$(basename "$f")
    hacl_file=($(find ~/hacl-star/dist/gcc-compatible/ -type f -name $file_name -not -path "*/hacl-star/dist/gcc-compatible/internal/*"))
    if [ $file_name != "Hacl_Ed25519.h" \
        -a $file_name != "Hacl_Ed25519.c" ]
    then
        continue
    fi  
    diff -u $hacl_file $f
done
