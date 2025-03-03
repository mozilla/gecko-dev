// META: global=window,worker

"use strict";

// The zstd-compressed bytes for the string "expected output".
const compressedZstdBytes = new Uint8Array([
  0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x79, 0x00, 0x00, 0x65, 0x78, 0x70, 0x65,
  0x63, 0x74, 0x65, 0x64, 0x20, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x5b, 0x11,
  0xc6, 0x85,
]);

for (let chunkSize = 1; chunkSize < 16; ++chunkSize) {
  promise_test(async t => {
    const ds = new DecompressionStream("zstd");

    const writer = ds.writable.getWriter();
    const reader = ds.readable.getReader();

    const writePromises = [];

    for (
      let beginning = 0;
      beginning < compressedZstdBytes.length;
      beginning += chunkSize
    ) {
      writePromises.push(
        writer.write(
          compressedZstdBytes.slice(beginning, beginning + chunkSize)
        )
      );
    }

    const writerClosePromise = writer.close();
    const chunks = [];

    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      chunks.push(value);
    }

    await Promise.all(writePromises);
    await writerClosePromise;

    let totalLength = 0;
    for (const chunk of chunks) {
      totalLength += chunk.byteLength;
    }

    const combined = new Uint8Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
      combined.set(chunk, offset);
      offset += chunk.byteLength;
    }

    const decompressedText = new TextDecoder().decode(combined);
    assert_equals(
      decompressedText,
      "expected output",
      `Decompressed text with chunkSize=${chunkSize} should match the expected output.`
    );
  }, `decompressing valid zstd input in chunks of size ${chunkSize} should yield "expected output"`);
}
