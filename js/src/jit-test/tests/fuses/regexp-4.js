// |jit-test| --fast-warmup; --no-threads

// Test invalidation of Ion code when RegExpPrototype fuse is popped for
// RegExpExec intrinsic.
function test() {  
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    re.test(s);
    if (i === 150) {
      // Pop the fuse.
      RegExp.prototype.exec = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 49);
}
test();
