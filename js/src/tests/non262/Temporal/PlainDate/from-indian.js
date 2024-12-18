// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal"))

// https://github.com/unicode-org/icu4x/issues/5070

let fromIso = new Temporal.PlainDate(2000, 12, 31, "indian");

let fromIndian = Temporal.PlainDate.from({
  calendar: "indian",
  year: fromIso.year,
  month: fromIso.month,
  day: fromIso.day,
});

assertEq(fromIndian.equals(fromIso), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
