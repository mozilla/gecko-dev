// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

const TypedArrayByteOffset = getSelfHostedValue("TypedArrayByteOffset");

function testTypedArrayByteOffset() {
  let ab = new ArrayBuffer(100).transferToImmutable();
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayByteOffset(ta), i & 3);
  }
}
testTypedArrayByteOffset();
