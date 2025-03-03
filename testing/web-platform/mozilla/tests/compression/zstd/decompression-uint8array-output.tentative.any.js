// META: global=window,worker

"use strict";

// The zstd-compressed bytes for the string "expected output".
const zstdChunkValue = new Uint8Array([
  0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x79, 0x00, 0x00, 0x65, 0x78, 0x70, 0x65,
  0x63, 0x74, 0x65, 0x64, 0x20, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x5b, 0x11,
  0xc6, 0x85,
]);

promise_test(async t => {
  const ds = new DecompressionStream("zstd");
  const writer = ds.writable.getWriter();
  const reader = ds.readable.getReader();

  const writePromise = writer.write(zstdChunkValue);
  const { value } = await reader.read();

  await writePromise;
  await writer.close();

  assert_equals(
    value.constructor.name,
    "Uint8Array",
    "The constructor should be Uint8Array"
  );
}, "decompressing zstd output should give Uint8Array chunks");
