// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat"))

var df = new Intl.DurationFormat("en", {
  seconds: "numeric",
  fractionalDigits: 9,
});

//   10_000_000 + (1 / 10^9)
// = 10000000.000000001
var duration = {
  seconds: 10_000_000,
  nanoseconds: 1,
};
assertEq(df.format(duration), "10000000.000000001");

// Number.MAX_SAFE_INTEGER = 9007199254740991
var duration = {
  seconds: Number.MAX_SAFE_INTEGER,
};
assertEq(df.format(duration), "9007199254740991.000000000");

// Number.MAX_SAFE_INTEGER * 1_000 = 9007199254740990976
var duration = {
  milliseconds: Number.MAX_SAFE_INTEGER * 1_000,
};
assertEq(df.format(duration), "9007199254740990.976000000");

// Number.MAX_SAFE_INTEGER * 1_000_000 = 9007199254740990951424
var duration = {
  microseconds: Number.MAX_SAFE_INTEGER * 1_000_000,
};
assertEq(df.format(duration), "9007199254740990.951424000");

// Number.MAX_SAFE_INTEGER * 1_000_000_000 = 9007199254740990926258176
var duration = {
  nanoseconds: Number.MAX_SAFE_INTEGER * 1_000_000_000,
};
assertEq(df.format(duration), "9007199254740990.926258176");

//   9007199254740991 + (9007199254740991 / 10^3) + (9007199254740991 / 10^6) + (9007199254740991 / 10^9)
// = 9.016215470202185986731991 × 10^15
var duration = {
  seconds: Number.MAX_SAFE_INTEGER,
  milliseconds: Number.MAX_SAFE_INTEGER,
  microseconds: Number.MAX_SAFE_INTEGER,
  nanoseconds: Number.MAX_SAFE_INTEGER,
};
assertThrowsInstanceOf(() => df.format(duration), RangeError);

var duration = {
  seconds: Number.MIN_SAFE_INTEGER,
  milliseconds: Number.MIN_SAFE_INTEGER,
  microseconds: Number.MIN_SAFE_INTEGER,
  nanoseconds: Number.MIN_SAFE_INTEGER,
};
assertThrowsInstanceOf(() => df.format(duration), RangeError);

//   1 + (2 / 10^3) + (3 / 10^6) + (9007199254740991 / 10^9)
// = 9.007200256743991 × 10^6
var duration = {
  seconds: 1,
  milliseconds: 2,
  microseconds: 3,
  nanoseconds: Number.MAX_SAFE_INTEGER,
};
assertEq(df.format(duration), "9007200.256743991");

//   (4503599627370497024 / 10^3) + (4503599627370494951424 / 10^6)
// = 4503599627370497.024 + 4503599627370494.951424
// = 9007199254740991.975424
var duration = {
  milliseconds: 4503599627370497_000,
  microseconds: 4503599627370495_000000,
};
assertEq(df.format(duration), "9007199254740991.975424000");

if (typeof reportCompare === "function")
  reportCompare(true, true);
