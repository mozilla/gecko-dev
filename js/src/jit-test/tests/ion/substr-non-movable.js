// |jit-test| --fast-warmup; --ion-offthread-compile=off

function f(from, expected) {
  for (let i = 0; i < 100; ++i) {
    assertEq("abcdefgh".slice(from), expected);
  }   
}
f(1, "bcdefgh");
f(-1, "h");
