// |jit-test| include:wasm.js;

loadRelativeToScript("load-mod.js");

// Limit of 1 million recursion groups
wasmFailValidateBinary(loadMod("wasm-gc-limits-r1M1-t1.wasm"), /too many/);
