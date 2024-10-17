function testDateGetTime() {
  var timeValues = [
    -1000,
    +1000,
    0,
    NaN,
  ];

  for (var i = 0; i < 250; ++i) {
    var t = timeValues[i & 3];
    var d = new Date(t);
    assertEq(d.getTime(), t);
    assertEq(d.valueOf(), t);
  }
}
testDateGetTime();
