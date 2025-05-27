// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

function fillArrayBuffer(ab) {
  let fill = new Int8Array(ab);
  for (let i = 0; i < fill.length; ++i) fill[i] = i + 1;
}

function test() {
  let ab = new ArrayBuffer(4);

  fillArrayBuffer(ab);

  let ta = new Int8Array(ab.transferToImmutable(), 2, 2);

  assertEq(ta[0], 3);
  assertEq(ta[1], 4);

  // Request GC to move inline data.
  gc();

  assertEq(ta[0], 3);
  assertEq(ta[1], 4);
}
test();
