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
];

function test(TA1, TA2) {
  var ta1 = new TA1(32);
  var ta2 = new TA2(ta1.buffer);
  ta1.set(ta2);
}

for (let TA1 of TypedArrays) {
  for (let TA2 of TypedArrays) {
    if (TA1.BYTES_PER_ELEMENT <= TA2.BYTES_PER_ELEMENT) {
      test(TA1, TA2);
    }
  }
}
