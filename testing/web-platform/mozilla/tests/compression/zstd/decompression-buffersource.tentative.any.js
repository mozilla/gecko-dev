// META: global=window,worker

"use strict";

// The zstd-compressed bytes for the string "expected output!!!!".
// Note that four exclamation points are added to make the compressed data a multiple of 8 bytes,
// which is a requirement to test with Float64Array.
const compressedZstdBytes = [
  0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x99, 0x00, 0x00, 0x65, 0x78, 0x70, 0x65,
  0x63, 0x74, 0x65, 0x64, 0x20, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x21, 0x21,
  0x21, 0x21, 0x2e, 0x4f, 0xe5, 0x2b,
];

const bufferSourceChunksForZstd = [
  {
    name: "ArrayBuffer",
    value: new Uint8Array(compressedZstdBytes).buffer,
  },
  {
    name: "Int8Array",
    value: new Int8Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Uint8Array",
    value: new Uint8Array(compressedZstdBytes),
  },
  {
    name: "Uint8ClampedArray",
    value: new Uint8ClampedArray(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Int16Array",
    value: new Int16Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Uint16Array",
    value: new Uint16Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Int32Array",
    value: new Int32Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Uint32Array",
    value: new Uint32Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Float16Array",
    value: new Float16Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Float32Array",
    value: new Float32Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "Float64Array",
    value: new Float64Array(new Uint8Array(compressedZstdBytes).buffer),
  },
  {
    name: "DataView",
    value: new DataView(new Uint8Array(compressedZstdBytes).buffer),
  },
];

for (const chunk of bufferSourceChunksForZstd) {
  promise_test(async t => {
    const ds = new DecompressionStream("zstd");
    const writer = ds.writable.getWriter();
    const reader = ds.readable.getReader();

    const writePromise = writer.write(chunk.value);
    const writerClosePromise = writer.close();

    const { value, done } = await reader.read();

    assert_false(done, "Stream should not be done after reading valid data");

    await writePromise;
    await writerClosePromise;

    const decompressedText = new TextDecoder().decode(value);
    assert_equals(
      decompressedText,
      "expected output!!!!",
      `The decompressed text should match the expected text when reading from ${chunk.name}`
    );
  }, `chunk of type ${chunk.name} should work for zstd decompression`);
}
