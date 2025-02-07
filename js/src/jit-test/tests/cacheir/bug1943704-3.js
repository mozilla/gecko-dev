let x = {};
x[NaN] = "nan";
x[-1.5] = "negative";
x[1.5] = "positive";

function foo(k) {
  return x[k];
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  assertEq(foo(NaN), "nan");
  assertEq(foo(1.5), "positive");
}
assertEq(foo(-1.5), "negative");
