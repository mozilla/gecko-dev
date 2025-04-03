// |jit-test| skip-if: !globalThis.SharedArrayBuffer

streamCacheEntry(new DataView(new SharedArrayBuffer()))
