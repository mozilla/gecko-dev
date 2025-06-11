function test1(v) {
  var undefined = v;
  assertEq(v === undefined, true);
  assertEq(v !== undefined, false);
  (function inner(a) {
    assertEq(a === undefined, true);
    assertEq(a !== undefined, false);
  })(v);
}
test1(1);

function test2() {
  var envChainObject = {undefined: 1};
  evaluate(`var x = 1; var res1 = x === undefined; var res2 = x !== undefined;`,
           {envChainObject});
  assertEq(envChainObject.res1, true);
  assertEq(envChainObject.res2, false);
}
test2();
