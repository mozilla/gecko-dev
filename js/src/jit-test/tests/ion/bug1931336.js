// GVN would alias the second Object.keys(obj).length to the first instance
// while the instruction are not supposed to alias due to the delete operation
// which exist in between.
function foo() {
  let obj = {a: 1};
  if (Object.keys(obj).length == 0) {}
  delete obj.a;
  assertEq(Object.keys(obj).length, 0);
}

with ({}) {}
for (var i = 0; i < 10000; i++) {
  foo();
}

