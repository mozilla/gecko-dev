#!/bin/bash
# Script to auto-generate the different archive types under the archives directory.

cd archives

rm archive.*

tar cf archive.tar -C reference .
gzip -fk archive.tar >archive.tar.gz
bzip2 -fk archive.tar >archive.tar.bz2
xz -zk archive.tar
cd reference && zip ../archive.zip -r * && cd ..
