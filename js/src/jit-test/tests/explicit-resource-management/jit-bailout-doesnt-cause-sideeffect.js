// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

function foo(d, o) {
  using x = d;
  return o.prop;
}

let count = 0;
let disp = {
  [Symbol.dispose]() { count++; }
}
for (let i = 0; i < 2000; i++) {
  foo(disp, { prop: 1});
}
foo(disp, {a: 1, prop: 2});
assertEq(count, 2001);
