/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var Vec3u32Type = TypedObject.uint32.array(3);

function foo_u32() {
  for (var i = 0; i < 30000; i += 3) {
    var vec = new Vec3u32Type([i, i+1, i+2]);
    var sum = vec[0] + vec[1] + vec[2];
    assertEq(sum, 3*i + 3);
  }
}

foo_u32();
