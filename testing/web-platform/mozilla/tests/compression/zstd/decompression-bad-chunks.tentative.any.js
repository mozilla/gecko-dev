// META: global=window,worker

"use strict";

const badChunks = [
  {
    name: "undefined",
    value: undefined,
  },
  {
    name: "null",
    value: null,
  },
  {
    name: "numeric",
    value: 3.14,
  },
  {
    name: "object, not BufferSource",
    value: {},
  },
  {
    name: "array",
    value: [65],
  },
  {
    name: "SharedArrayBuffer",
    // Use a getter to postpone construction so that all tests don't fail where
    // SharedArrayBuffer is not yet implemented.
    get value() {
      // See https://github.com/whatwg/html/issues/5380 for why not `new SharedArrayBuffer()`
      return new WebAssembly.Memory({ shared: true, initial: 1, maximum: 1 })
        .buffer;
    },
  },
  {
    name: "shared Uint8Array",
    get value() {
      // See https://github.com/whatwg/html/issues/5380 for why not `new SharedArrayBuffer()`
      return new Uint8Array(
        new WebAssembly.Memory({ shared: true, initial: 1, maximum: 1 }).buffer
      );
    },
  },
  {
    name: "invalid zstd bytes",
    value: new Uint8Array([0x00, 0x01, 0x02, 0x03]),
  },
];

for (const badChunk of badChunks) {
  promise_test(async t => {
    const ds = new DecompressionStream("zstd");
    const reader = ds.readable.getReader();
    const writer = ds.writable.getWriter();

    writer.write(badChunk.value).catch(() => {});
    reader.read().catch(() => {});

    await promise_rejects_js(t, TypeError, writer.close(), "writer.close() should reject");
    await promise_rejects_js(t, TypeError, writer.closed, "write.closed should reject");

    await promise_rejects_js(t, TypeError, reader.read(), "reader.read() should reject");
    await promise_rejects_js(t, TypeError, reader.closed, "read.closed should reject");
  }, `"zstd" decompression for bad chunk of type "${badChunk.name}" should produce an error`);
}
