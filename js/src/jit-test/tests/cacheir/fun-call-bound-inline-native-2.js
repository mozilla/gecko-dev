// Test inlining bound natives through Function.prototype.call

function test(fn, expected) {
  for (let i = 0; i < 400; ++i) {
    let r = fn.call(null, 0, 1);
    assertEq(r, expected);
  }
}

for (let i = 0; i < 2; ++i) {
  let fn, expected;
  if (i === 0) {
    fn = Math.min.bind();
    expected = 0;
  } else {
    fn = Math.max.bind();
    expected = 1;
  }
  test(fn, expected);
}

function testBound(fn, expected) {
  for (let i = 0; i < 400; ++i) {
    let r = fn.call(null, 0, 1);
    assertEq(r, expected);
  }
}

for (let i = 0; i < 2; ++i) {
  let fn, expected;
  if (i === 0) {
    fn = Math.min.bind(null, -1);
    expected = -1;
  } else {
    fn = Math.max.bind(null, 2);
    expected = 2;
  }
  testBound(fn, expected);
}
