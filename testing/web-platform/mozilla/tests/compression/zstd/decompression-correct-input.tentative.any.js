// META: global=window,worker

"use strict";

// The zstd-compressed bytes for the string "expected output".
const compressedZstdBytes = new Uint8Array([
  0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x79, 0x00, 0x00, 0x65, 0x78, 0x70, 0x65,
  0x63, 0x74, 0x65, 0x64, 0x20, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x5b, 0x11,
  0xc6, 0x85,
]);

promise_test(async t => {
  const ds = new DecompressionStream("zstd");
  const writer = ds.writable.getWriter();
  const reader = ds.readable.getReader();

  const writePromise = writer.write(compressedZstdBytes);
  const writerClosePromise = writer.close();

  const { value, done } = await reader.read();

  assert_false(
    done,
    "The done flag should not be set after reading valid input"
  );

  await writePromise;
  await writerClosePromise;

  const decompressedText = new TextDecoder().decode(value);
  assert_equals(
    decompressedText,
    "expected output",
    "The decompressed text should match the expected text"
  );
}, "decompressing valid zstd input should yield 'expected output'");
