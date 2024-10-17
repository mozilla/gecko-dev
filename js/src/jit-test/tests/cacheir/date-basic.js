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

var dateValues = [
  // Start of the epoch and start of the year date.
  [1970, 1-1, 1, 4],

  // End of year date.
  [2023, 12-1, 31, 0],

  // Date near maximum allowed time value (275760 September, 13).
  [275760, 9-1, 13 - 1, 5],

  // Date near minimum allowed time value (-271821 April, 20).
  [-271821, 4-1, 20 + 1, 3],

  // Invalid Date.
  [NaN, NaN, NaN, NaN],
];

function testDateGetFullYear() {
  for (var i = 0; i < 250; ++i) {
    var [year, month, date] = dateValues[i % dateValues.length];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(year, month, date);

    // First call to getFullYear initializes the cache.
    assertEq(d.getFullYear(), year);

    // Second call to getFullYear uses the cached value.
    assertEq(d.getFullYear(), year);
  }
}
testDateGetFullYear();

function testDateGetMonth() {
  for (var i = 0; i < 250; ++i) {
    var [year, month, date] = dateValues[i % dateValues.length];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(year, month, date);

    // First call to getMonth initializes the cache.
    assertEq(d.getMonth(), month);

    // Second call to getMonth uses the cached value.
    assertEq(d.getMonth(), month);
  }
}
testDateGetMonth();

function testDateGetDate() {
  for (var i = 0; i < 250; ++i) {
    var [year, month, date] = dateValues[i % dateValues.length];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(year, month, date);

    // First call to getDate initializes the cache.
    assertEq(d.getDate(), date);

    // Second call to getDate uses the cached value.
    assertEq(d.getDate(), date);
  }
}
testDateGetDate();

function testDateGetDay() {
  for (var i = 0; i < 250; ++i) {
    var [year, month, date, day] = dateValues[i % dateValues.length];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(year, month, date);

    // First call to getDay initializes the cache.
    assertEq(d.getDay(), day);

    // Second call to getDay uses the cached value.
    assertEq(d.getDay(), day);
  }
}
testDateGetDay();

function testDateGetFullYearMonthDateDay() {
  for (var i = 0; i < 250; ++i) {
    var [year, month, date, day] = dateValues[i % dateValues.length];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(year, month, date);

    // Test calling different methods, too.
    assertEq(d.getFullYear(), year);
    assertEq(d.getMonth(), month);
    assertEq(d.getDate(), date);
    assertEq(d.getDay(), day);
  }
}
testDateGetFullYearMonthDateDay();

function testDateGetHours() {
  var timeValues = [
    0,
    12,
    23,
    NaN,
  ];

  for (var i = 0; i < 250; ++i) {
    var t = timeValues[i & 3];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(2000, 0, 1, t);

    // First call to getHours initializes the cache.
    assertEq(d.getHours(), t);

    // Second call to getHours uses the cached value.
    assertEq(d.getHours(), t);
  }
}
testDateGetHours();

function testDateGetMinutes() {
  var timeValues = [
    0,
    30,
    59,
    NaN,
  ];

  for (var i = 0; i < 250; ++i) {
    var t = timeValues[i & 3];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(2000, 0, 1, 0, t);

    // First call to getMinutes initializes the cache.
    assertEq(d.getMinutes(), t);

    // Second call to getMinutes uses the cached value.
    assertEq(d.getMinutes(), t);
  }
}
testDateGetMinutes();

function testDateGetSeconds() {
  var timeValues = [
    0,
    30,
    59,
    NaN,
  ];

  for (var i = 0; i < 250; ++i) {
    var t = timeValues[i & 3];

    // Create a new Date object with an uninitialized local time cache.
    var d = new Date(2000, 0, 1, 0, 0, t);

    // First call to getSeconds initializes the cache.
    assertEq(d.getSeconds(), t);

    // Second call to getSeconds uses the cached value.
    assertEq(d.getSeconds(), t);
  }
}
testDateGetSeconds();
