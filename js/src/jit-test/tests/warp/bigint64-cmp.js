const i64 = new BigInt64Array([
  -0x8000_0000_0000_0000n,
  -0x7fff_ffff_ffff_ffffn,

  -0x8000_0000n,
  -0x7fff_ffffn,

  -1n,
  0n,
  1n,

  0x7fff_ffffn,
  0x8000_0000n,

  0x7fff_ffff_ffff_ffffn,
  0x8000_0000_0000_0000n,
]);

const u64 = new BigUint64Array([
  0n,
  1n,

  0x7fff_ffffn,
  0x8000_0000n,

  0xffff_ffffn,
  0x1_0000_0000n,

  0x7fff_ffff_ffff_ffffn,
  0x8000_0000_0000_0000n,
  0xffff_ffff_ffff_ffffn,
]);

function gcd(a, b) {
  a |= 0;
  b |= 0;
  while (b !== 0) {
    [a, b] = [b, a % b];
  }
  return Math.abs(a);
}

function assertAllCombinationsTested(xs, ys, n) {
  // If the array lengths are relatively prime and their product is at least
  // |n| long, all possible combinations are tested at least once. Make sure
  // we test each combination at least three times.
  var m = 3;

  assertEq(gcd(xs.length, ys.length), 1);
  assertEq(m * xs.length * ys.length <= n, true);
}

function fillWithZeros(ta) {
  let length = ta.length;
  let zeros = 1;
  while (gcd(length, length + zeros) !== 1) {
    zeros++;
  }

  let result = new ta.constructor(length + zeros);
  result.set(ta);
  return result;
}

function testI64() {
  for (var i = 0; i < 200; ++i) {
    var v = i64[i % i64.length];

    // Cast to Int128 to ensure non-optimized BigInt comparison is used.
    var eq_zero = v == BigInt.asIntN(128, 0n);
    var lt_zero = v < BigInt.asIntN(128, 0n);

    var eq_one = v == BigInt.asIntN(128, 1n);
    var lt_one = v < BigInt.asIntN(128, 1n);

    var eq_neg_one = v == BigInt.asIntN(128, -1n);
    var lt_neg_one = v < BigInt.asIntN(128, -1n);

    var eq_i31 = v == BigInt.asIntN(128, 0x8000_0000n);
    var lt_i31 = v < BigInt.asIntN(128, 0x8000_0000n);

    var eq_i32 = v == BigInt.asIntN(128, 0x1_0000_0000n);
    var lt_i32 = v < BigInt.asIntN(128, 0x1_0000_0000n);

    // BigInt constant
    assertEq(v == 0n, eq_zero);
    assertEq(v != 0n, !eq_zero);
    assertEq(v < 0n, lt_zero && !eq_zero);
    assertEq(v <= 0n, lt_zero || eq_zero);
    assertEq(v > 0n, !lt_zero && !eq_zero);
    assertEq(v >= 0n, !lt_zero || eq_zero);

    assertEq(v == 1n, eq_one);
    assertEq(v != 1n, !eq_one);
    assertEq(v < 1n, lt_one && !eq_one);
    assertEq(v <= 1n, lt_one || eq_one);
    assertEq(v > 1n, !lt_one && !eq_one);
    assertEq(v >= 1n, !lt_one || eq_one);

    assertEq(v == -1n, eq_neg_one);
    assertEq(v != -1n, !eq_neg_one);
    assertEq(v < -1n, lt_neg_one && !eq_neg_one);
    assertEq(v <= -1n, lt_neg_one || eq_neg_one);
    assertEq(v > -1n, !lt_neg_one && !eq_neg_one);
    assertEq(v >= -1n, !lt_neg_one || eq_neg_one);

    assertEq(v == 0x8000_0000n, eq_i31);
    assertEq(v != 0x8000_0000n, !eq_i31);
    assertEq(v < 0x8000_0000n, lt_i31 && !eq_i31);
    assertEq(v <= 0x8000_0000n, lt_i31 || eq_i31);
    assertEq(v > 0x8000_0000n, !lt_i31 && !eq_i31);
    assertEq(v >= 0x8000_0000n, !lt_i31 || eq_i31);

    assertEq(v == 0x1_0000_0000n, eq_i32);
    assertEq(v != 0x1_0000_0000n, !eq_i32);
    assertEq(v < 0x1_0000_0000n, lt_i32 && !eq_i32);
    assertEq(v <= 0x1_0000_0000n, lt_i32 || eq_i32);
    assertEq(v > 0x1_0000_0000n, !lt_i32 && !eq_i32);
    assertEq(v >= 0x1_0000_0000n, !lt_i32 || eq_i32);

    // Int32 constant
    assertEq(v == 0, eq_zero);
    assertEq(v != 0, !eq_zero);
    assertEq(v < 0, lt_zero && !eq_zero);
    assertEq(v <= 0, lt_zero || eq_zero);
    assertEq(v > 0, !lt_zero && !eq_zero);
    assertEq(v >= 0, !lt_zero || eq_zero);

    assertEq(v == 1, eq_one);
    assertEq(v != 1, !eq_one);
    assertEq(v < 1, lt_one && !eq_one);
    assertEq(v <= 1, lt_one || eq_one);
    assertEq(v > 1, !lt_one && !eq_one);
    assertEq(v >= 1, !lt_one || eq_one);

    assertEq(v == -1, eq_neg_one);
    assertEq(v != -1, !eq_neg_one);
    assertEq(v < -1, lt_neg_one && !eq_neg_one);
    assertEq(v <= -1, lt_neg_one || eq_neg_one);
    assertEq(v > -1, !lt_neg_one && !eq_neg_one);
    assertEq(v >= -1, !lt_neg_one || eq_neg_one);

    // BigInt constant too large for I64.
    assertEq(v == 0x8000_0000_0000_0000n, false);
    assertEq(v != 0x8000_0000_0000_0000n, true);
    assertEq(v < 0x8000_0000_0000_0000n, true);
    assertEq(v <= 0x8000_0000_0000_0000n, true);
    assertEq(v > 0x8000_0000_0000_0000n, false);
    assertEq(v >= 0x8000_0000_0000_0000n, false);

    assertEq(v == -0x8000_0000_0000_0001n, false);
    assertEq(v != -0x8000_0000_0000_0001n, true);
    assertEq(v < -0x8000_0000_0000_0001n, false);
    assertEq(v <= -0x8000_0000_0000_0001n, false);
    assertEq(v > -0x8000_0000_0000_0001n, true);
    assertEq(v >= -0x8000_0000_0000_0001n, true);

    assertEq(v == 0x1_0000_0000_0000_0000n, false);
    assertEq(v != 0x1_0000_0000_0000_0000n, true);
    assertEq(v < 0x1_0000_0000_0000_0000n, true);
    assertEq(v <= 0x1_0000_0000_0000_0000n, true);
    assertEq(v > 0x1_0000_0000_0000_0000n, false);
    assertEq(v >= 0x1_0000_0000_0000_0000n, false);
  }
}
testI64();

function testU64() {
  for (var i = 0; i < 200; ++i) {
    var v = u64[i % u64.length];

    // Cast to Uint128 to ensure non-optimized BigInt comparison is used.
    var eq_zero = v == BigInt.asUintN(128, 0n);
    var lt_zero = v < BigInt.asUintN(128, 0n);

    var eq_one = v == BigInt.asUintN(128, 1n);
    var lt_one = v < BigInt.asUintN(128, 1n);

    var eq_i31 = v == BigInt.asUintN(128, 0x8000_0000n);
    var lt_i31 = v < BigInt.asUintN(128, 0x8000_0000n);

    var eq_i32 = v == BigInt.asUintN(128, 0x1_0000_0000n);
    var lt_i32 = v < BigInt.asUintN(128, 0x1_0000_0000n);

    // BigInt constant
    assertEq(v == 0n, eq_zero);
    assertEq(v != 0n, !eq_zero);
    assertEq(v < 0n, lt_zero && !eq_zero);
    assertEq(v <= 0n, lt_zero || eq_zero);
    assertEq(v > 0n, !lt_zero && !eq_zero);
    assertEq(v >= 0n, !lt_zero || eq_zero);

    assertEq(v == 1n, eq_one);
    assertEq(v != 1n, !eq_one);
    assertEq(v < 1n, lt_one && !eq_one);
    assertEq(v <= 1n, lt_one || eq_one);
    assertEq(v > 1n, !lt_one && !eq_one);
    assertEq(v >= 1n, !lt_one || eq_one);

    assertEq(v == 0x8000_0000n, eq_i31);
    assertEq(v != 0x8000_0000n, !eq_i31);
    assertEq(v < 0x8000_0000n, lt_i31 && !eq_i31);
    assertEq(v <= 0x8000_0000n, lt_i31 || eq_i31);
    assertEq(v > 0x8000_0000n, !lt_i31 && !eq_i31);
    assertEq(v >= 0x8000_0000n, !lt_i31 || eq_i31);

    assertEq(v == 0x1_0000_0000n, eq_i32);
    assertEq(v != 0x1_0000_0000n, !eq_i32);
    assertEq(v < 0x1_0000_0000n, lt_i32 && !eq_i32);
    assertEq(v <= 0x1_0000_0000n, lt_i32 || eq_i32);
    assertEq(v > 0x1_0000_0000n, !lt_i32 && !eq_i32);
    assertEq(v >= 0x1_0000_0000n, !lt_i32 || eq_i32);

    // Int32 constant
    assertEq(v == 0, eq_zero);
    assertEq(v != 0, !eq_zero);
    assertEq(v < 0, lt_zero && !eq_zero);
    assertEq(v <= 0, lt_zero || eq_zero);
    assertEq(v > 0, !lt_zero && !eq_zero);
    assertEq(v >= 0, !lt_zero || eq_zero);

    assertEq(v == 1, eq_one);
    assertEq(v != 1, !eq_one);
    assertEq(v < 1, lt_one && !eq_one);
    assertEq(v <= 1, lt_one || eq_one);
    assertEq(v > 1, !lt_one && !eq_one);
    assertEq(v >= 1, !lt_one || eq_one);

    // BigInt constant too large for U64.
    assertEq(v == 0x1_0000_0000_0000_0000n, false);
    assertEq(v != 0x1_0000_0000_0000_0000n, true);
    assertEq(v < 0x1_0000_0000_0000_0000n, true);
    assertEq(v <= 0x1_0000_0000_0000_0000n, true);
    assertEq(v > 0x1_0000_0000_0000_0000n, false);
    assertEq(v >= 0x1_0000_0000_0000_0000n, false);

    // Negative BigInt constant
    assertEq(v == -1n, false);
    assertEq(v != -1n, true);
    assertEq(v < -1n, false);
    assertEq(v <= -1n, false);
    assertEq(v > -1n, true);
    assertEq(v >= -1n, true);

    // Negative Int32 constant
    assertEq(v == -1, false);
    assertEq(v != -1, true);
    assertEq(v < -1, false);
    assertEq(v <= -1, false);
    assertEq(v > -1, true);
    assertEq(v >= -1, true);
  }
}
testU64();

// Compare Int64 against Int64.
function testII64() {
  var r64 = fillWithZeros(i64);
  assertAllCombinationsTested(i64, r64, 500);

  for (var i = 0; i < 500; ++i) {
    var v = i64[i % i64.length];
    var w = r64[i % r64.length];

    // Cast to Int128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asIntN(128, w);
    var lt = v < BigInt.asIntN(128, w);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testII64();

// Compare Uint64 against Uint64.
function testUU64() {
  var r64 = fillWithZeros(u64);
  assertAllCombinationsTested(u64, r64, 500);

  for (var i = 0; i < 500; ++i) {
    var v = u64[i % u64.length];
    var w = r64[i % r64.length];

    // Cast to Uint128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asUintN(128, w);
    var lt = v < BigInt.asUintN(128, w);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testUU64();

// Compare Int64 against Uint64.
function testIU64() {
  var r64 = new BigUint64Array(i64.buffer);
  for (var i = 0; i < 200; ++i) {
    var v = i64[i % i64.length];
    var w = r64[i % r64.length];

    // Cast to Uint128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asUintN(128, v);
    var lt = v < BigInt.asUintN(128, v);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testIU64();

// Compare Uint64 against Int64.
function testUI64() {
  var r64 = new BigUint64Array(u64.buffer);
  for (var i = 0; i < 200; ++i) {
    var v = u64[i % u64.length];
    var w = r64[i % r64.length];

    // Cast to Int128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asIntN(128, v);
    var lt = v < BigInt.asIntN(128, v);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testUI64();

// Compare Int64 against IntPtr.
function testI64IPtr() {
  var r64 = fillWithZeros(i64);
  assertAllCombinationsTested(i64, r64, 500);

  for (var i = 0; i < 500; ++i) {
    var v = i64[i % i64.length];
    var w = r64[i % r64.length];

    // Apply an operation to execute BigInt as IntPtr codepaths.
    w = BigInt.asIntN(32, w);
    var x = w < 0 ? 1n : w > 0 ? -1n : 0n;
    w += x;

    // Cast to Int128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asIntN(128, w);
    var lt = v < BigInt.asIntN(128, w);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testI64IPtr();

// Compare IntPtr against Int64.
function testIPtrI64() {
  var r64 = fillWithZeros(i64);
  assertAllCombinationsTested(i64, r64, 500);

  for (var i = 0; i < 500; ++i) {
    var v = i64[i % i64.length];
    var w = r64[i % r64.length];

    // Apply an operation to execute BigInt as IntPtr codepaths.
    v = BigInt.asIntN(32, v);
    var x = v < 0 ? 1n : v > 0 ? -1n : 0n;
    v += x;

    // Cast to Int128 to ensure non-optimized BigInt comparison is used.
    var eq = v == BigInt.asIntN(128, w);
    var lt = v < BigInt.asIntN(128, w);

    assertEq(v == w, eq);
    assertEq(v != w, !eq);
    assertEq(v < w, lt && !eq);
    assertEq(v <= w, lt || eq);
    assertEq(v > w, !lt && !eq);
    assertEq(v >= w, !lt || eq);
  }
}
testIPtrI64();
