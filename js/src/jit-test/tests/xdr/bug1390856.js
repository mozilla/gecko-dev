// |jit-test| skip-if: helperThreadCount() === 0

// Test main thread encode/decode OOM
oomTest(function() {
    let t = cacheEntry(`function f() { function g() { }; return 3; };`);

    evaluate(t, { sourceIsLazy: true, saveBytecodeWithDelazifications: true });
    evaluate(t, { sourceIsLazy: true, readBytecode: true });
});
