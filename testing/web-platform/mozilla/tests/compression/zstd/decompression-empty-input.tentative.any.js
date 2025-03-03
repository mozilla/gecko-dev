// META: global=window,worker

"use strict";

// The zstd-compressed bytes for an empty string "".
const compressedZstdBytes = new Uint8Array([
  0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x00, 0x01, 0x00, 0x00, 0x99, 0xe9, 0xd8, 0x51,
]);

promise_test(async t => {
  const ds = new DecompressionStream("zstd");
  const writer = ds.writable.getWriter();
  const reader = ds.readable.getReader();

  const writePromise = writer.write(compressedZstdBytes);
  const writerClosePromise = writer.close();

  const { value, done } = await reader.read();

  assert_true(done, "The done flag should be set after reading empty input");

  await writePromise;
  await writerClosePromise;

  const decompressedText = new TextDecoder().decode(value);
  assert_equals(
    decompressedText,
    "",
    "The decompressed text should match the expected text"
  );
}, "decompressing empty zstd input should yield an empty string");
