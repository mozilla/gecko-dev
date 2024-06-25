#!/bin/bash
cp -r "fuzz/corpus/fuzz_$1" "fuzz/corpus/fuzz_$1_iter_0"
i=0
while true; do
  j=$((i + 1))
  cp -r "fuzz/corpus/fuzz_$1_iter_${i}" "fuzz/corpus/fuzz_$1_iter_${i}.bak"
  mkdir "fuzz/corpus/fuzz_$1_iter_${j}"
  cargo fuzz cmin --all-features "fuzz_$1" "fuzz/corpus/fuzz_$1_iter_${i}" -- \
    -dict=fuzz/fuzz.dict -max_len=70000 "fuzz/corpus/fuzz_$1_iter_${j}"
  diff "fuzz/corpus/fuzz_$1_iter_${i}.bak" "fuzz/corpus/fuzz_$1_iter_${j}"
  if $?; then
    # Last iteration made no difference, so we're done
    rm -r "fuzz/corpus/fuzz_$1"
    mv "fuzz/corpus/fuzz_$1_iter_${j}" "fuzz/corpus/fuzz_$1"
  fi
  i=$j
done
