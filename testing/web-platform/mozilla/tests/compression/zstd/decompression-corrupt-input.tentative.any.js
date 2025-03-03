// META: global=window,worker

"use strict";

// The zstd-compressed bytes for the string "expected output"
// where the optional checksum value has been included in the data.
//
// Each section of the data is labeled according to RFC 8878.
// https://datatracker.ietf.org/doc/html/rfc8878
const compressedZstdBytesWithChecksum = new Uint8Array([
  // Section 3.1.1 - Magic Number (4 bytes)
  0x28, 0xb5, 0x2f, 0xfd,
  // Section 3.1.1.1 - Frame Header (2 to 14 bytes)
  0x04,       // Section 3.1.1.1.1 - Frame Header Descriptor
  0x58,       // Section 3.1.1.1.2 - Window Descriptor
  0x79, 0x00, // Section 3.1.1.1.3 - Dictionary ID
  // Section 3.1.1.2 - Block Data (16 bytes)
  0x00, 0x65, 0x78, 0x70,
  0x65, 0x63, 0x74, 0x65,
  0x64, 0x20, 0x6f, 0x75,
  0x74, 0x70, 0x75, 0x74,
  // Section 3.1.1 - Content Checksum (4 bytes)
  0x5b, 0x11, 0xc6, 0x85,
]);

// The zstd-compressed bytes for the string "expected output",
// where the optional checksum value has been omitted from the data.
//
// Each section of the data is labeled according to RFC 8878.
// https://datatracker.ietf.org/doc/html/rfc8878
const compressedZstdBytesNoChecksum = new Uint8Array([
  // Section 3.1.1 - Magic Number (4 bytes)
  0x28, 0xb5, 0x2f, 0xfd,
  // Section 3.1.1.1 - Frame Header (2 to 14 bytes)
  0x00,       // Section 3.1.1.1.1 - Frame Header Descriptor
  0x58,       // Section 3.1.1.1.2 - Window Descriptor
  0x79, 0x00, // Section 3.1.1.1.3 - Dictionary ID
  // Section 3.1.1.2 - Block Data (16 bytes)
  0x00, 0x65, 0x78, 0x70,
  0x65, 0x63, 0x74, 0x65,
  0x64, 0x20, 0x6f, 0x75,
  0x74, 0x70, 0x75, 0x74,
]);

// We define all tests with fields:
//
//  - name: descriptive string
//  - type: "unchanged", "extendStart", "extendEnd", "truncateStart", "truncateEnd", or "field"
//  - removeBytes / extraBytes / offset / length / value as needed
//
//  - expectedResultWithChecksum: "success" | "error" | "corrupt"
//  - expectedResultNoChecksum: "success" | "error" | "corrupt"
//    (if omitted/undefined, the no-checksum path won't be tested)
//
const expectations = [
  {
    name: "Unchanged",
    type: "unchanged",
    expectedResultWithChecksum: "success",
    expectedResultNoChecksum: "success",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Truncate 1 byte from start",
    type: "truncateStart",
    removeBytes: 1,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Truncate 1 byte from end",
    type: "truncateEnd",
    removeBytes: 1,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Truncate 2 bytes from start",
    type: "truncateStart",
    removeBytes: 2,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Truncate 2 bytes from end",
    type: "truncateEnd",
    removeBytes: 2,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from start with [0x00]",
    type: "extendStart",
    extraBytes: [0x00],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from end with [0x00]",
    type: "extendEnd",
    extraBytes: [0x00],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from end [0xff]",
    type: "extendEnd",
    extraBytes: [0xff],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from start with [0xff]",
    type: "extendStart",
    extraBytes: [0xff],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from end with [0xff]",
    type: "extendEnd",
    extraBytes: [0xff],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from start with [0x00, 0x00]",
    type: "extendStart",
    extraBytes: [0x00, 0x00],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Extend from end with [0x00, 0x00]",
    type: "extendEnd",
    extraBytes: [0x00, 0x00],
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Magic Number: offset=0 replace with 0xff",
    type: "field",
    offset: 0,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Magic Number: offset=1 replace with 0xff",
    type: "field",
    offset: 1,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Magic Number: offset=2 replace with 0xff",
    type: "field",
    offset: 2,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Magic Number: offset=3 replace with 0xff",
    type: "field",
    offset: 3,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Frame Header Descriptor: offset=4 replace with 0x05",
    type: "field",
    offset: 4,
    length: 1,
    value: 0x05,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Window Descriptor: offset=5 replace with 0xff",
    type: "field",
    offset: 5,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Dictionary ID: offset=6 length=2 replace with 0x00",
    type: "field",
    offset: 6,
    length: 2,
    value: 0x00,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "error",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Block Data: offset=18 replace with 0x64",
    type: "field",
    offset: 18,
    length: 1,
    value: 0x64,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "corrupt",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  {
    name: "Block Data: offset=23 replace with 0x73",
    type: "field",
    offset: 23,
    length: 1,
    value: 0x73,
    expectedResultWithChecksum: "error",
    expectedResultNoChecksum: "corrupt",
    compressedZstdBytesWithChecksum,
    compressedZstdBytesNoChecksum,
  },
  // The following test cases mutate the checksum itself, so there are no expected
  // results for the no-checksum scenario.
  {
    name: "Content Checksum: offset=-4 replace with 0x00",
    type: "field",
    offset: -4,
    length: 1,
    value: 0x00,
    expectedResultWithChecksum: "error",
    compressedZstdBytesWithChecksum,
  },
  {
    name: "Content Checksum: offset=-3 replace with 0xff",
    type: "field",
    offset: -3,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    compressedZstdBytesWithChecksum,
  },
  {
    name: "Content Checksum: offset=-2 replace with 0x00",
    type: "field",
    offset: -2,
    length: 1,
    value: 0x00,
    expectedResultWithChecksum: "error",
    compressedZstdBytesWithChecksum,
  },
  {
    name: "Content Checksum: offset=-1 replace with 0xff",
    type: "field",
    offset: -1,
    length: 1,
    value: 0xff,
    expectedResultWithChecksum: "error",
    compressedZstdBytesWithChecksum,
  },
];

async function tryDecompress(input) {
  const ds = new DecompressionStream("zstd");
  const reader = ds.readable.getReader();
  const writer = ds.writable.getWriter();

  const writePromise = writer.write(input).catch(() => {});
  const writerClosePromise = writer.close().catch(() => {});

  let out = [];
  while (true) {
    try {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      out = out.concat(Array.from(value));
    } catch (e) {
      if (e.name === "TypeError") {
        return { result: "error" };
      }
      return { result: e.name };
    }
  }

  await writePromise;
  await writerClosePromise;

  const textDecoder = new TextDecoder();
  const text = textDecoder.decode(new Uint8Array(out));
  if (text !== "expected output") {
    return { result: "corrupt" };
  }
  return { result: "success" };
}

function produceTestInput(testCase, compressedBytes) {
  switch (testCase.type) {
    case "unchanged":
      return compressedBytes;

    case "truncateStart": {
      return compressedBytes.slice(testCase.removeBytes);
    }
    case "truncateEnd": {
      return compressedBytes.slice(
        0,
        compressedBytes.length - testCase.removeBytes
      );
    }
    case "extendStart": {
      return new Uint8Array([...testCase.extraBytes, ...compressedBytes]);
    }
    case "extendEnd": {
      return new Uint8Array([...compressedBytes, ...testCase.extraBytes]);
    }
    case "field": {
      const output = new Uint8Array(compressedBytes);
      let realOffset = testCase.offset;
      if (realOffset < 0) {
        realOffset = output.length + realOffset;
      }
      for (let i = 0; i < testCase.length; i++) {
        output[realOffset + i] = testCase.value;
      }
      return output;
    }
    default: {
      throw new Error(`Unknown testCase type: ${testCase.type}`);
    }
  }
}

for (const testCase of expectations) {
  promise_test(async t => {
    const inputWithChecksum = produceTestInput(
      testCase,
      testCase.compressedZstdBytesWithChecksum
    );
    const { result: resultWithChecksum } =
      await tryDecompress(inputWithChecksum);

    assert_equals(
      resultWithChecksum,
      testCase.expectedResultWithChecksum,
      `${testCase.name} (with checksum)`
    );

    let resultNoChecksum;
    if (testCase.expectedResultNoChecksum !== undefined) {
      const inputNoChecksum = produceTestInput(
        testCase,
        testCase.compressedZstdBytesNoChecksum
      );
      const { result } = await tryDecompress(inputNoChecksum);
      resultNoChecksum = result;
    }

    if (testCase.expectedResultNoChecksum !== undefined) {
      assert_equals(
        resultNoChecksum,
        testCase.expectedResultNoChecksum,
        `${testCase.name} (no checksum)`
      );
    }
  }, testCase.name);
}
