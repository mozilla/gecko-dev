// |jit-test| --enable-arraybuffer-immutable; skip-if: !ArrayBuffer.prototype.transferToImmutable

const scopes = [
  "SameProcess",
  "DifferentProcess",
  "DifferentProcessForIndexedDB",
];

function testInt32Array(scope) {
  var length = 4;
  var byteLength = length * Int32Array.BYTES_PER_ELEMENT;

  var ab = new ArrayBuffer(byteLength);
  assertEq(ab.immutable, false);
  assertEq(ab.byteLength, byteLength);

  var ta = new Int32Array(ab);
  ta.set([1, 87654321, -123]);

  var ita1 = new Int32Array(ab.transferToImmutable());
  assertEq(ita1.byteLength, byteLength);
  assertEq(ita1.toString(), "1,87654321,-123,0");
  assertEq(ita1.buffer.immutable, true);

  var clonebuf = serialize(ita1, undefined, {scope});
  var ita2 = deserialize(clonebuf);
  assertEq(ita2 instanceof Int32Array, true);
  assertEq(ita2.byteLength, byteLength);
  assertEq(ita2.toString(), "1,87654321,-123,0");
  assertEq(ita2.buffer.immutable, true);
  assertEq(ita2.buffer.byteLength, byteLength);
}
scopes.forEach(testInt32Array);

function testFloat64Array(scope) {
  var length = 4;
  var byteLength = length * Float64Array.BYTES_PER_ELEMENT;

  var ab = new ArrayBuffer(byteLength);
  assertEq(ab.immutable, false);
  assertEq(ab.byteLength, byteLength);

  var ta = new Float64Array(ab);
  ta.set([NaN, 3.14, 0, 0]);

  var ita1 = new Float64Array(ab.transferToImmutable());
  assertEq(ita1.byteLength, byteLength);
  assertEq(ita1.toString(), "NaN,3.14,0,0");
  assertEq(ita1.buffer.immutable, true);

  var clonebuf = serialize(ita1, undefined, {scope});
  var ita2 = deserialize(clonebuf);
  assertEq(ita2 instanceof Float64Array, true);
  assertEq(ita2.byteLength, byteLength);
  assertEq(ita2.toString(), "NaN,3.14,0,0");
  assertEq(ita2.buffer.immutable, true);
  assertEq(ita2.buffer.byteLength, byteLength);
}
scopes.forEach(testFloat64Array);

function testDataView(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;

  var ab = new ArrayBuffer(byteLength);
  assertEq(ab.immutable, false);
  assertEq(ab.byteLength, byteLength);

  var ta = new Uint8Array(ab);
  ta.set([5, 0, 255]);
  assertEq(ta.toString(), "5,0,255,0");

  var idv1 = new DataView(ab.transferToImmutable());
  assertEq(idv1.byteLength, byteLength);
  assertEq(idv1.buffer.immutable, true);

  var clonebuf = serialize(idv1, undefined, {scope});
  var idv2 = deserialize(clonebuf);
  assertEq(idv2 instanceof DataView, true);
  assertEq(idv2.byteLength, byteLength);
  assertEq(new Uint8Array(idv2.buffer).toString(), "5,0,255,0");
  assertEq(idv2.buffer.immutable, true);
  assertEq(idv2.buffer.byteLength, byteLength);
}
scopes.forEach(testDataView);

function testArrayBuffer(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;

  var ab = new ArrayBuffer(byteLength);
  assertEq(ab.immutable, false);
  assertEq(ab.byteLength, byteLength);

  var ta = new Uint8Array(ab);
  ta.set([33, 44, 55, 66]);
  assertEq(ta.toString(), "33,44,55,66");

  var iab1 = ab.transferToImmutable();

  var clonebuf = serialize(iab1, undefined, {scope});
  var iab2 = deserialize(clonebuf);
  assertEq(iab2 instanceof ArrayBuffer, true);
  assertEq(new Uint8Array(iab2).toString(), "33,44,55,66");
  assertEq(iab2.immutable, true);
  assertEq(iab2.byteLength, byteLength);
}
scopes.forEach(testArrayBuffer);
