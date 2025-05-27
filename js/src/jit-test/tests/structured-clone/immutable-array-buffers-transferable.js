// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

load(libdir + "asserts.js");

const scopes = [
  "SameProcess",
  "DifferentProcess",
  "DifferentProcessForIndexedDB",
];

function testArrayBufferTransferable(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;

  var ab = new ArrayBuffer(byteLength);
  assertEq(ab.immutable, false);
  assertEq(ab.byteLength, byteLength);

  var ta = new Uint8Array(ab);
  ta.set([33, 44, 55, 66]);

  var iab = ab.transferToImmutable();
  var ita = new Uint8Array(iab);

  assertEq(ab.detached, true);
  assertEq(iab.detached, false);
  assertEq(ita.toString(), "33,44,55,66");

  assertThrowsInstanceOf(() => serialize(iab, [iab], {scope}), TypeError);

  assertEq(ab.detached, true);
  assertEq(iab.detached, false);
  assertEq(ita.toString(), "33,44,55,66");
}
scopes.forEach(testArrayBufferTransferable);
