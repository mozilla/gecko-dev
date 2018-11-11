// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;
var Bool64x2 = SIMD.Bool64x2;

function replaceLaneN(laneIndex, arr, value) {
    var copy = arr.slice();
    assertEq(laneIndex <= arr.length, true);
    copy[laneIndex] = value;
    return copy;
}

var replaceLane0 = replaceLaneN.bind(null, 0);
var replaceLane1 = replaceLaneN.bind(null, 1);
var replaceLane2 = replaceLaneN.bind(null, 2);
var replaceLane3 = replaceLaneN.bind(null, 3);
var replaceLane4 = replaceLaneN.bind(null, 4);
var replaceLane5 = replaceLaneN.bind(null, 5);
var replaceLane6 = replaceLaneN.bind(null, 6);
var replaceLane7 = replaceLaneN.bind(null, 7);
var replaceLane8 = replaceLaneN.bind(null, 8);
var replaceLane9 = replaceLaneN.bind(null, 9);
var replaceLane10 = replaceLaneN.bind(null, 10);
var replaceLane11 = replaceLaneN.bind(null, 11);
var replaceLane12 = replaceLaneN.bind(null, 12);
var replaceLane13 = replaceLaneN.bind(null, 13);
var replaceLane14 = replaceLaneN.bind(null, 14);
var replaceLane15 = replaceLaneN.bind(null, 15);

function testReplaceLane(vec, scalar, simdFunc, func) {
    var varr = simdToArray(vec);
    var observed = simdToArray(simdFunc(vec, scalar));
    var expected = func(varr, scalar);
    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}

function test() {
  function testType(type, inputs) {
      var length = simdToArray(inputs[0][0]).length;
      for (var [vec, s] of inputs) {
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 0, y), replaceLane0);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 1, y), replaceLane1);
          if (length <= 2)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 2, y), replaceLane2);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 3, y), replaceLane3);
          if (length <= 4)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 4, y), replaceLane4);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 5, y), replaceLane5);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 6, y), replaceLane6);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 7, y), replaceLane7);
          if (length <= 8)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 8, y), replaceLane8);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 9, y), replaceLane9);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 10, y), replaceLane10);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 11, y), replaceLane11);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 12, y), replaceLane12);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 13, y), replaceLane13);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 14, y), replaceLane14);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 15, y), replaceLane15);
      }
  }

  function TestError(){};
  var good = {valueOf: () => 42};
  var bad = {valueOf: () => {throw new TestError(); }};

  var Float32x4inputs = [
      [Float32x4(1, 2, 3, 4), 5],
      [Float32x4(1.87, 2.08, 3.84, 4.17), Math.fround(13.37)],
      [Float32x4(NaN, -0, Infinity, -Infinity), 0]
  ];
  testType('Float32x4', Float32x4inputs);

  var v = Float32x4inputs[1][0];
  assertEqX4(Float32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX4(Float32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 4, good), RangeError);
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 1.1, good), RangeError);

  var Float64x2inputs = [
      [Float64x2(1, 2), 5],
      [Float64x2(1.87, 2.08), Math.fround(13.37)],
      [Float64x2(NaN, -0), 0]
  ];
  testType('Float64x2', Float64x2inputs);

  var v = Float64x2inputs[1][0];
  assertEqX2(Float64x2.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX2(Float64x2.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 2, good), RangeError);
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 1.1, good), RangeError);

  var Int8x16inputs = [[Int8x16(0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, INT8_MIN, INT8_MAX), 17]];
  testType('Int8x16', Int8x16inputs);

  var v = Int8x16inputs[0][0];
  assertEqX16(Int8x16.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX16(Int8x16.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 16, good), RangeError);
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 1.1, good), RangeError);

  var Int16x8inputs = [[Int16x8(0, 1, 2, 3, -1, -2, INT16_MIN, INT16_MAX), 9]];
  testType('Int16x8', Int16x8inputs);

  var v = Int16x8inputs[0][0];
  assertEqX8(Int16x8.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX8(Int16x8.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 8, good), RangeError);
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 1.1, good), RangeError);

  var Int32x4inputs = [
      [Int32x4(1, 2, 3, 4), 5],
      [Int32x4(INT32_MIN, INT32_MAX, 3, 4), INT32_MIN],
  ];
  testType('Int32x4', Int32x4inputs);

  var v = Int32x4inputs[1][0];
  assertEqX4(Int32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX4(Int32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 4, good), RangeError);
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 1.1, good), RangeError);

  var Uint8x16inputs = [[Uint8x16(0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, INT8_MIN, UINT8_MAX), 17]];
  testType('Uint8x16', Uint8x16inputs);

  var v = Uint8x16inputs[0][0];
  assertEqX16(Uint8x16.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX16(Uint8x16.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Uint8x16.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Uint8x16.replaceLane(v, 16, good), RangeError);
  assertThrowsInstanceOf(() => Uint8x16.replaceLane(v, 1.1, good), RangeError);

  var Uint16x8inputs = [[Uint16x8(0, 1, 2, 3, -1, -2, INT16_MIN, UINT16_MAX), 9]];
  testType('Uint16x8', Uint16x8inputs);

  var v = Uint16x8inputs[0][0];
  assertEqX8(Uint16x8.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX8(Uint16x8.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Uint16x8.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Uint16x8.replaceLane(v, 8, good), RangeError);
  assertThrowsInstanceOf(() => Uint16x8.replaceLane(v, 1.1, good), RangeError);

  var Uint32x4inputs = [
      [Uint32x4(1, 2, 3, 4), 5],
      [Uint32x4(INT32_MIN, UINT32_MAX, INT32_MAX, 4), UINT32_MAX],
  ];
  testType('Uint32x4', Uint32x4inputs);

  var v = Uint32x4inputs[1][0];
  assertEqX4(Uint32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX4(Uint32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Uint32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Uint32x4.replaceLane(v, 4, good), RangeError);
  assertThrowsInstanceOf(() => Uint32x4.replaceLane(v, 1.1, good), RangeError);

  var Bool64x2inputs = [
      [Bool64x2(true, true), false],
  ];
  testType('Bool64x2', Bool64x2inputs);

  var v = Bool64x2inputs[0][0];
  assertEqX2(Bool64x2.replaceLane(v, 0),       replaceLane0(simdToArray(v), false));
  assertEqX2(Bool64x2.replaceLane(v, 0, true), replaceLane0(simdToArray(v), true));
  assertEqX2(Bool64x2.replaceLane(v, 0, bad),  replaceLane0(simdToArray(v), true));
  assertThrowsInstanceOf(() => Bool64x2.replaceLane(v, 4, true), RangeError);
  assertThrowsInstanceOf(() => Bool64x2.replaceLane(v, 1.1, false), RangeError);

  var Bool32x4inputs = [
      [Bool32x4(true, true, true, true), false],
  ];
  testType('Bool32x4', Bool32x4inputs);

  var v = Bool32x4inputs[0][0];
  assertEqX4(Bool32x4.replaceLane(v, 0),       replaceLane0(simdToArray(v), false));
  assertEqX4(Bool32x4.replaceLane(v, 0, true), replaceLane0(simdToArray(v), true));
  assertEqX4(Bool32x4.replaceLane(v, 0, bad),  replaceLane0(simdToArray(v), true));
  assertThrowsInstanceOf(() => Bool32x4.replaceLane(v, 4, true), RangeError);
  assertThrowsInstanceOf(() => Bool32x4.replaceLane(v, 1.1, false), RangeError);

  var Bool16x8inputs = [
      [Bool16x8(true, true, true, true, true, true, true, true), false],
  ];

  testType('Bool16x8', Bool16x8inputs);
  var v = Bool16x8inputs[0][0];
  assertEqX8(Bool16x8.replaceLane(v, 0),       replaceLane0(simdToArray(v), false));
  assertEqX8(Bool16x8.replaceLane(v, 0, true), replaceLane0(simdToArray(v), true));
  assertEqX8(Bool16x8.replaceLane(v, 0, bad),  replaceLane0(simdToArray(v), true));
  assertThrowsInstanceOf(() => Bool16x8.replaceLane(v, 16, true), RangeError);
  assertThrowsInstanceOf(() => Bool16x8.replaceLane(v, 1.1, false), RangeError);

  var Bool8x16inputs = [
      [Bool8x16(true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true), false],
  ];

  testType('Bool8x16', Bool8x16inputs);
  var v = Bool8x16inputs[0][0];
  assertEqX16(Bool8x16.replaceLane(v, 0),       replaceLane0(simdToArray(v), false));
  assertEqX16(Bool8x16.replaceLane(v, 0, true), replaceLane0(simdToArray(v), true));
  assertEqX16(Bool8x16.replaceLane(v, 0, bad),  replaceLane0(simdToArray(v), true));
  assertThrowsInstanceOf(() => Bool8x16.replaceLane(v, 16, true), RangeError);
  assertThrowsInstanceOf(() => Bool8x16.replaceLane(v, 1.1, false), RangeError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
