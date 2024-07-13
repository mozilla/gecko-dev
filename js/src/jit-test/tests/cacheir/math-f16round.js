function testExact() {
  var values = [
    0,
    0.5,
    1,
    100.25,
    Infinity,
    NaN,
  ];

  for (var i = 0; i < 1000; ++i) {
    var x = values[i % values.length];
    assertEq(Math.f16round(x), x);
    assertEq(-Math.f16round(-x), x);
    assertEq(Math.f16round(-x), -x);
    assertEq(-Math.f16round(x), -x);
  }
}
for (let i = 0; i < 2; ++i) testExact();

function testInexact() {
  var values = [
    0.1,
    Math.SQRT2,
    65519,
    65520
  ];
  var expected = [
    0.0999755859375,
    1.4140625,
    65504,
    Infinity,
  ];

  for (var i = 0; i < 1000; ++i) {
    var j = i % values.length;
    var x = values[j];
    var y = expected[j];
    assertEq(Math.f16round(x), y);
    assertEq(-Math.f16round(-x), y);
    assertEq(Math.f16round(-x), -y);
    assertEq(-Math.f16round(x), -y);
  }
}
for (let i = 0; i < 2; ++i) testInexact();
