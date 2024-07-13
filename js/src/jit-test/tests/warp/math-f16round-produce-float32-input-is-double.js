function sqrt_float32(v) {
  return Math.fround(Math.sqrt(Math.fround(v)));
}

function sqrt_float16(v) {
  return Math.fround(Math.sqrt(Math.f16round(v)));
}

function abs_float32(v) {
  return Math.fround(Math.abs(Math.fround(v)));
}

function abs_float16(v) {
  return Math.fround(Math.abs(Math.f16round(v)));
}

function ceil_float32(v) {
  return Math.fround(Math.abs(Math.fround(v)));
}

function ceil_float16(v) {
  return Math.fround(Math.abs(Math.f16round(v)));
}

function mul_float32(v) {
  return Math.fround(Math.fround(v) * 2);
}

function mul_float16(v) {
  return Math.fround(Math.f16round(v) * 2);
}

function cmp_float32(v) {
  return Math.fround(v) < 1000;
}

function cmp_float16(v) {
  return Math.f16round(v) < 1000;
}

// Don't inline functions into the global scope.
with ({}) ;

for (let i = 0; i < 1024; ++i) {
  assertEq(sqrt_float16(i + 0.5), sqrt_float32(i + 0.5));
  assertEq(abs_float16(i + 0.5), abs_float32(i + 0.5));
  assertEq(ceil_float16(i + 0.5), ceil_float32(i + 0.5));
  assertEq(mul_float16(i + 0.5), mul_float32(i + 0.5));
  assertEq(cmp_float16(i + 0.5), cmp_float32(i + 0.5));
}
