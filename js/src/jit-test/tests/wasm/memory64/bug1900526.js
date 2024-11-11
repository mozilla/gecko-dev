// |jit-test| heavy; allow-oom; skip-if: !canRunHugeMemoryTests()

const maxPages = wasmMaxMemoryPages("i64");
const m = new WebAssembly.Memory({initial: 0n, address: "i64"});
try {
    m.grow(BigInt(maxPages));
    assertEq(m.buffer.byteLength, maxPages * PageSizeInBytes);
} catch (e) {
    assertEq(e.message.includes("failed to grow"), true, `got error: ${e}`); // OOM
}
