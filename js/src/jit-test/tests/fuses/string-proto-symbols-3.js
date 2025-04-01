// |jit-test| --fast-warmup; --no-threads

// Test invalidation of Ion code when StringPrototypeSymbols fuse is popped.
function test() {  
  var s = "foobar";
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replace("abc", "").replace("def", "");
    if (i === 150) {
      // Pop the fuse.
      Object.prototype[Symbol.replace] = function() {
        count++;
        return s;
      };
    }
  }
  assertEq(count, 98);
}
test();
