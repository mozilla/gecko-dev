// Int64 always in range of Int32. No Int64ToBigIntPtr bailout on 32- and 64-bit.
function i64InI32Range() {
  var ta = new BigInt64Array([
    -0x8000_0000n,
    -0x7fff_ffffn,
    -2n,
    -1n,
    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigInt64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i < N; ++i) {
    var x = ta[i % ta.length];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
}
for (var i = 0; i < 2; ++i) i64InI32Range();

// Int64 not in range of Int32. Int64ToBigIntPtr bailout on 32-bit.
function i64NotInI32RangeBailout() {
  var ta = new BigInt64Array([
    // Ensure not interpreted as signed Int32 value.
    0x8000_0000n,

    -0x8000_0000n,
    -0x7fff_ffffn,
    -2n,
    -1n,
    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigInt64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i <= N; ++i) {
    // Use index zero when |i == N|, but make sure all code paths are always
    // executed to ensure there are no cold path bailouts.
    var index = ((1 + (i % (ta.length - 1))) * (i < N))|0;

    var x = ta[index];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
  assertEq(index, 0);
  assertEq(x, 0x8000_0000n);
}
for (var i = 0; i < 2; ++i) i64NotInI32RangeBailout();

// Int64 not in range of Int32. Int64ToBigIntPtr bailout on 32-bit.
function i64NotInI32RangeBailout2() {
  var ta = new BigInt64Array([
    0x8000_0000_0000_0000n,

    -0x8000_0000n,
    -0x7fff_ffffn,
    -2n,
    -1n,
    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigInt64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i <= N; ++i) {
    // Use index zero when |i == N|, but make sure all code paths are always
    // executed to ensure there are no cold path bailouts.
    var index = ((1 + (i % (ta.length - 1))) * (i < N))|0;

    var x = ta[index];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
  assertEq(index, 0);
  assertEq(x, -0x8000_0000_0000_0000n);
}
for (var i = 0; i < 2; ++i) i64NotInI32RangeBailout2();

// UInt64 always in range of Int32. No Int64ToBigIntPtr bailout on 32- and 64-bit.
function u64InI32Range() {
  var ta = new BigUint64Array([
    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigUint64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i < N; ++i) {
    var x = ta[i % ta.length];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
}
for (var i = 0; i < 2; ++i) u64InI32Range();

// UInt64 not in range of Int32. Int64ToBigIntPtr bailout on 32-bit.
function u64NotInI32RangeBailout() {
  var ta = new BigUint64Array([
    0x8000_0000n,

    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigUint64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i <= N; ++i) {
    // Use index zero when |i == N|, but make sure all code paths are always
    // executed to ensure there are no cold path bailouts.
    var index = ((1 + (i % (ta.length - 1))) * (i < N))|0;

    var x = ta[index];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
  assertEq(index, 0);
  assertEq(x, 0x8000_0000n);
}
for (var i = 0; i < 2; ++i) u64NotInI32RangeBailout();

// UInt64 not in range of Int64. Int64ToBigIntPtr bailout on 32- and 64-bit.
function u64NotInI64RangeBailout() {
  var ta = new BigUint64Array([
    0x8000_0000_0000_0000n,

    0n,
    1n,
    2n,
    0x7fff_fffen,
    0x7fff_ffffn,
  ]);

  // Add operation to trigger BigIntPtr code path. Don't use constant 0n to
  // ensure compiler doesn't optimize away the addition.
  var zero = new BigUint64Array([
    0n, 0n,
  ]);

  var N = 200;
  for (var i = 0; i <= N; ++i) {
    // Use index zero when |i == N|, but make sure all code paths are always
    // executed to ensure there are no cold path bailouts.
    var index = ((1 + (i % (ta.length - 1))) * (i < N))|0;

    var x = ta[index];
    var y = zero[i & 1];
    var z = x + y;
    assertEq(z, x);
  }
  assertEq(index, 0);
  assertEq(x, 0x8000_0000_0000_0000n);
}
for (var i = 0; i < 2; ++i) u64NotInI64RangeBailout();
