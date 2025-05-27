// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

function testImmutableArrayBuffer() {
  for (let i = 0; i < 4; ++i) {
    let ab = new ArrayBuffer(i).transferToImmutable();
    for (let j = 0; j < 100; ++j) {
      assertEq(ab.byteLength, i);
    }
  }
}
for (let i = 0; i < 2; ++i) testImmutableArrayBuffer();
