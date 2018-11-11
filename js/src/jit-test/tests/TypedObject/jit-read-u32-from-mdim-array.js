/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var PointType = TypedObject.uint32.array(3);
var VecPointType = PointType.array(3);

function foo() {
  for (var i = 0; i < 10000; i += 9) {
    var vec = new VecPointType([
      [i,   i+1, i+2],
      [i+3, i+4, i+5],
      [i+6, i+7, i+8]]);
    var sum = vec[0][0] + vec[0][1] + vec[0][2];
    assertEq(sum, 3*i + 3);
    sum = vec[1][0] + vec[1][1] + vec[1][2];
    assertEq(sum, 3*i + 12);
    sum = vec[2][0] + vec[2][1] + vec[2][2];
    assertEq(sum, 3*i + 21);
  }
}

foo();
