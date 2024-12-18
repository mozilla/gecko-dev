// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal"))

// https://github.com/unicode-org/icu4x/issues/4914
assertThrowsInstanceOf(() => {
  let date = Temporal.PlainDate.from({
    calendar: "islamic-umalqura",
    year: -6823,
    monthCode: "M01",
    day: 1,
  });
  // assertEq(date.day, 1);
}, RangeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
