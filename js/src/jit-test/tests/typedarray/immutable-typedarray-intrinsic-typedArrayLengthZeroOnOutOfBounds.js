// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

load(libdir + "asserts.js");

const TypedArrayLengthZeroOnOutOfBounds = getSelfHostedValue("TypedArrayLengthZeroOnOutOfBounds");

function testTypedArrayLength() {
  let ab = new ArrayBuffer(100).transferToImmutable();
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayLengthZeroOnOutOfBounds(ta), 100 - (i & 3));
  }
}
testTypedArrayLength();
