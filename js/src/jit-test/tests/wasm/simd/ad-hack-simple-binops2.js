// |jit-test| skip-if: !wasmSimdEnabled()

// This test is very slow with GenerationalGC zeal mode, especially if
// CheckHeapBeforeMinorGC is also enabled. GenerationalGC makes the number of
// minor GCs go from 13 -> 50000.
if (this.unsetgczeal) {
    unsetgczeal("GenerationalGC");
}

// Do not include these in the preamble, they must be loaded after lib/wasm.js
load(scriptdir + "ad-hack-preamble.js")
load(scriptdir + "ad-hack-binop-preamble.js")

runSimpleBinopTest(2, 3);
