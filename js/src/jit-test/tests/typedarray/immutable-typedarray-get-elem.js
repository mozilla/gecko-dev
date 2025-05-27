// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

const TypedArrays = [
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Uint8ClampedArray,
  Float16Array,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array,
];

function test(TA) {
  const length = 4;

  let expected = new TA(length);
  let type = expected[0].constructor;

  for (let i = 0; i < length; ++i) {
    expected[i] = type(i * i);
  }

  let actual = new TA(expected.buffer.sliceToImmutable());
  assertEq(actual.buffer.immutable, true);

  // In-bounds access
  for (let i = 0; i < 200; ++i) {
    let index = i % length;
    assertEq(actual[index], expected[index]);
  }

  // Out-of-bounds access
  for (let i = 0; i < 200; ++i) {
    let index = i % (length + 4);
    assertEq(actual[index], expected[index]);
  }
}

for (let TA of TypedArrays) {
  // Copy test function to ensure monomorphic ICs.
  let copy = Function(`return ${test}`)();

  copy(TA);
}
