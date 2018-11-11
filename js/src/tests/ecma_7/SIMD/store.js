// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// As SIMD.*.store is entirely symmetric to SIMD.*.load, this file just
// contains basic tests to store on one single TypedArray kind, while load is
// exhaustively tested. See load.js for more details.

const POISON = 42;

function reset(ta) {
    for (var i = 0; i < ta.length; i++)
        ta[i] = POISON + i;
}

function assertChanged(ta, from, expected) {
    var i = 0;
    for (; i < from; i++)
        assertEq(ta[i], POISON + i);
    for (; i < from + expected.length; i++)
        assertEq(ta[i], expected[i - from]);
    for (; i < ta.length; i++)
        assertEq(ta[i], POISON + i);
}

function testStore(ta, kind, i, v) {
    var asArr = simdToArray(v);

    reset(ta);
    SIMD[kind].store(ta, i, v);
    assertChanged(ta, i, asArr);

    var length = asArr.length;
    if (length >= 8) // Int8x16 and Int16x8 only support store, and not store1/store2/etc.
        return;

    reset(ta);
    SIMD[kind].store1(ta, i, v);
    assertChanged(ta, i, [asArr[0]]);
    if (length > 2) {
        reset(ta);
        SIMD[kind].store2(ta, i, v);
        assertChanged(ta, i, [asArr[0], asArr[1]]);

        reset(ta);
        SIMD[kind].store3(ta, i, v);
        assertChanged(ta, i, [asArr[0], asArr[1], asArr[2]]);
    }
}

function testStoreInt8x16(Buffer) {
    var I8 = new Int8Array(new Buffer(32));

    var v = SIMD.Int8x16(0, 1, INT8_MAX, INT8_MIN, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    testStore(I8, 'Int8x16', 0, v);
    testStore(I8, 'Int8x16', 1, v);
    testStore(I8, 'Int8x16', 2, v);
    testStore(I8, 'Int8x16', 16, v);

    assertThrowsInstanceOf(() => SIMD.Int8x16.store(I8), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int8x16.store(I8, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int16x8.store(I8, 0, v), TypeError);
}

function testStoreInt16x8(Buffer) {
    var I16 = new Int16Array(new Buffer(64));

    var v = SIMD.Int16x8(0, 1, INT16_MAX, INT16_MIN, 4, 5, 6, 7);
    testStore(I16, 'Int16x8', 0, v);
    testStore(I16, 'Int16x8', 1, v);
    testStore(I16, 'Int16x8', 2, v);
    testStore(I16, 'Int16x8', 24, v);

    assertThrowsInstanceOf(() => SIMD.Int16x8.store(I16), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int16x8.store(I16, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int8x16.store(I16, 0, v), TypeError);
}

function testStoreInt32x4(Buffer) {
    var I32 = new Int32Array(new Buffer(64));

    var v = SIMD.Int32x4(0, 1, Math.pow(2,31) - 1, -Math.pow(2, 31));
    testStore(I32, 'Int32x4', 0, v);
    testStore(I32, 'Int32x4', 1, v);
    testStore(I32, 'Int32x4', 2, v);
    testStore(I32, 'Int32x4', 12, v);

    assertThrowsInstanceOf(() => SIMD.Int32x4.store(I32), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int32x4.store(I32, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Float32x4.store(I32, 0, v), TypeError);
}

function testStoreUint8x16(Buffer) {
    var I8 = new Uint8Array(new Buffer(32));

    var v = SIMD.Uint8x16(0, 1, INT8_MAX, INT8_MIN, UINT8_MAX, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    testStore(I8, 'Uint8x16', 0, v);
    testStore(I8, 'Uint8x16', 1, v);
    testStore(I8, 'Uint8x16', 2, v);
    testStore(I8, 'Uint8x16', 16, v);

    assertThrowsInstanceOf(() => SIMD.Uint8x16.store(I8), TypeError);
    assertThrowsInstanceOf(() => SIMD.Uint8x16.store(I8, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Uint16x8.store(I8, 0, v), TypeError);
}

function testStoreUint16x8(Buffer) {
    var I16 = new Uint16Array(new Buffer(64));

    var v = SIMD.Uint16x8(0, 1, INT16_MAX, INT16_MIN, 4, 5, 6, 7);
    testStore(I16, 'Uint16x8', 0, v);
    testStore(I16, 'Uint16x8', 1, v);
    testStore(I16, 'Uint16x8', 2, v);
    testStore(I16, 'Uint16x8', 24, v);

    assertThrowsInstanceOf(() => SIMD.Uint16x8.store(I16), TypeError);
    assertThrowsInstanceOf(() => SIMD.Uint16x8.store(I16, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Uint8x16.store(I16, 0, v), TypeError);
}

function testStoreUint32x4(Buffer) {
    var I32 = new Uint32Array(new Buffer(64));

    var v = SIMD.Uint32x4(0, 1, Math.pow(2,31) - 1, -Math.pow(2, 31));
    testStore(I32, 'Uint32x4', 0, v);
    testStore(I32, 'Uint32x4', 1, v);
    testStore(I32, 'Uint32x4', 2, v);
    testStore(I32, 'Uint32x4', 12, v);

    assertThrowsInstanceOf(() => SIMD.Uint32x4.store(I32), TypeError);
    assertThrowsInstanceOf(() => SIMD.Uint32x4.store(I32, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Float32x4.store(I32, 0, v), TypeError);
}

function testStoreFloat32x4(Buffer) {
    var F32 = new Float32Array(new Buffer(64));

    var v = SIMD.Float32x4(1,2,3,4);
    testStore(F32, 'Float32x4', 0, v);
    testStore(F32, 'Float32x4', 1, v);
    testStore(F32, 'Float32x4', 2, v);
    testStore(F32, 'Float32x4', 12, v);

    var v = SIMD.Float32x4(NaN, -0, -Infinity, 5e-324);
    testStore(F32, 'Float32x4', 0, v);
    testStore(F32, 'Float32x4', 1, v);
    testStore(F32, 'Float32x4', 2, v);
    testStore(F32, 'Float32x4', 12, v);

    assertThrowsInstanceOf(() => SIMD.Float32x4.store(F32), TypeError);
    assertThrowsInstanceOf(() => SIMD.Float32x4.store(F32, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Int32x4.store(F32, 0, v), TypeError);
}

function testStoreFloat64x2(Buffer) {
    var F64 = new Float64Array(new Buffer(128));

    var v = SIMD.Float64x2(1, 2);
    testStore(F64, 'Float64x2', 0, v);
    testStore(F64, 'Float64x2', 1, v);
    testStore(F64, 'Float64x2', 14, v);

    var v = SIMD.Float64x2(NaN, -0);
    testStore(F64, 'Float64x2', 0, v);
    testStore(F64, 'Float64x2', 1, v);
    testStore(F64, 'Float64x2', 14, v);

    var v = SIMD.Float64x2(-Infinity, +Infinity);
    testStore(F64, 'Float64x2', 0, v);
    testStore(F64, 'Float64x2', 1, v);
    testStore(F64, 'Float64x2', 14, v);

    assertThrowsInstanceOf(() => SIMD.Float64x2.store(F64), TypeError);
    assertThrowsInstanceOf(() => SIMD.Float64x2.store(F64, 0), TypeError);
    assertThrowsInstanceOf(() => SIMD.Float32x4.store(F64, 0, v), TypeError);
}

function testSharedArrayBufferCompat() {
    var I32 = new Int32Array(new SharedArrayBuffer(16*4));
    var TA = I32;

    var I8 = new Int8Array(TA.buffer);
    var I16 = new Int16Array(TA.buffer);
    var F32 = new Float32Array(TA.buffer);
    var F64 = new Float64Array(TA.buffer);

    var Int8x16 = SIMD.Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var Int16x8 = SIMD.Int16x8(1, 2, 3, 4, 5, 6, 7, 8);
    var Int32x4 = SIMD.Int32x4(1, 2, 3, 4);
    var Uint8x16 = SIMD.Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    var Uint16x8 = SIMD.Uint16x8(1, 2, 3, 4, 5, 6, 7, 8);
    var Uint32x4 = SIMD.Uint32x4(1, 2, 3, 4);
    var Float32x4 = SIMD.Float32x4(1, 2, 3, 4);
    var Float64x2 = SIMD.Float64x2(1, 2);

    for (var ta of [
                    new Uint8Array(TA.buffer),
                    new Int8Array(TA.buffer),
                    new Uint16Array(TA.buffer),
                    new Int16Array(TA.buffer),
                    new Uint32Array(TA.buffer),
                    new Int32Array(TA.buffer),
                    new Float32Array(TA.buffer),
                    new Float64Array(TA.buffer)
                   ])
    {
        SIMD.Int8x16.store(ta, 0, Int8x16);
        for (var i = 0; i < 16; i++) assertEq(I8[i], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16][i]);

        SIMD.Int16x8.store(ta, 0, Int16x8);
        for (var i = 0; i < 8; i++) assertEq(I16[i], [1, 2, 3, 4, 5, 6, 7, 8][i]);

        SIMD.Int32x4.store(ta, 0, Int32x4);
        for (var i = 0; i < 4; i++) assertEq(I32[i], [1, 2, 3, 4][i]);

        SIMD.Uint8x16.store(ta, 0, Uint8x16);
        for (var i = 0; i < 16; i++) assertEq(I8[i], [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16][i]);

        SIMD.Uint16x8.store(ta, 0, Uint16x8);
        for (var i = 0; i < 8; i++) assertEq(I16[i], [1, 2, 3, 4, 5, 6, 7, 8][i]);

        SIMD.Uint32x4.store(ta, 0, Uint32x4);
        for (var i = 0; i < 4; i++) assertEq(I32[i], [1, 2, 3, 4][i]);

        SIMD.Float32x4.store(ta, 0, Float32x4);
        for (var i = 0; i < 4; i++) assertEq(F32[i], [1, 2, 3, 4][i]);

        SIMD.Float64x2.store(ta, 0, Float64x2);
        for (var i = 0; i < 2; i++) assertEq(F64[i], [1, 2][i]);

        assertThrowsInstanceOf(() => SIMD.Int8x16.store(ta, 1024, Int8x16), RangeError);
        assertThrowsInstanceOf(() => SIMD.Int16x8.store(ta, 1024, Int16x8), RangeError);
        assertThrowsInstanceOf(() => SIMD.Int32x4.store(ta, 1024, Int32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.Float32x4.store(ta, 1024, Float32x4), RangeError);
        assertThrowsInstanceOf(() => SIMD.Float64x2.store(ta, 1024, Float64x2), RangeError);
    }
}

testStoreInt8x16(ArrayBuffer);
testStoreInt16x8(ArrayBuffer);
testStoreInt32x4(ArrayBuffer);
testStoreUint8x16(ArrayBuffer);
testStoreUint16x8(ArrayBuffer);
testStoreUint32x4(ArrayBuffer);
testStoreFloat32x4(ArrayBuffer);
testStoreFloat64x2(ArrayBuffer);

if (typeof SharedArrayBuffer != "undefined") {
  testStoreInt8x16(SharedArrayBuffer);
  testStoreInt16x8(SharedArrayBuffer);
  testStoreInt32x4(SharedArrayBuffer);
  testStoreUint8x16(SharedArrayBuffer);
  testStoreUint16x8(SharedArrayBuffer);
  testStoreUint32x4(SharedArrayBuffer);
  testStoreFloat32x4(SharedArrayBuffer);
  testStoreFloat64x2(SharedArrayBuffer);
  testSharedArrayBufferCompat();
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
