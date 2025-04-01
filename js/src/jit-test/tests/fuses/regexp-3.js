// |jit-test| --fast-warmup; --no-threads

// Test invalidation of Ion code when RegExpPrototype fuse is popped.
function test() {  
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replace(re, "").replace(re, "");
    if (i === 150) {
      // Pop the fuse.
      RegExp.prototype.exec = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 98);
}
test();
