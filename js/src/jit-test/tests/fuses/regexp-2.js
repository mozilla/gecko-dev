// Tests for IsRegExpPrototypeOptimizable and IsOptimizableRegExpObject intrinsics.
function test() {
  const IsRegExpPrototypeOptimizable = getSelfHostedValue("IsRegExpPrototypeOptimizable");
  const IsOptimizableRegExpObject = getSelfHostedValue("IsOptimizableRegExpObject");
  
  for (var i = 0; i < 20; i++) {
    assertEq(IsRegExpPrototypeOptimizable(), true);
    assertEq(IsOptimizableRegExpObject(/a/), true);
    assertEq(IsOptimizableRegExpObject({}), false);

    // Proto is not RegExp.prototype.
    var re1 = /abc/;
    assertEq(IsOptimizableRegExpObject(re1), true);
    Object.setPrototypeOf(re1, Object.create(RegExp.prototype));
    assertEq(IsOptimizableRegExpObject(re1), false);

    // Own "flags" property.
    var re2 = /abc.*def/;
    assertEq(IsOptimizableRegExpObject(re2), true);
    Object.defineProperty(re2, "flags", {value: ""});
    assertEq(IsOptimizableRegExpObject(re2), false);

    // Other own properties.
    var re3 = /.+/gi;
    assertEq(IsOptimizableRegExpObject(re3), true);
    re3.foobar = 1;
    assertEq(IsOptimizableRegExpObject(re3), false);
  }

  for (var i = 0; i < 20; i++) {
    if (i === 13) {
      // This pops the fuse!
      RegExp.prototype.exec = function() {};
    }

    assertEq(IsRegExpPrototypeOptimizable(), i < 13);
    assertEq(IsOptimizableRegExpObject(/abc.*/), i < 13);

    var re = /abc/;
    Object.setPrototypeOf(re, Object.create(RegExp.prototype));
    assertEq(IsOptimizableRegExpObject(re), false);
  }
}
test();
