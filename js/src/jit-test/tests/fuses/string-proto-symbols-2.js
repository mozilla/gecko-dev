// Test for CanOptimizeStringProtoSymbolLookup intrinsic.
function test() {
  const CanOptimizeStringProtoSymbolLookup = getSelfHostedValue("CanOptimizeStringProtoSymbolLookup");
  for (var i = 0; i < 55; i++) {
    if (i === 10) {
      String.prototype.foo = "bar";
      Object.prototype.hello = "world";
    }
    if (i === 40) {
      // Pops the fuse.
      Object.prototype[Symbol.replace] = 123;
    }
    assertEq(CanOptimizeStringProtoSymbolLookup(), i < 40);
  }
}
test();
