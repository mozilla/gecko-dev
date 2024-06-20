// |reftest| skip-if(!this.hasOwnProperty("Temporal"))

// Equivalent monthCode and month are resolved to the same PlainMonthDay.
{
  let withMonthCode = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    year: 2023,
    monthCode: "M02",
    day: 30,
  });

  let withMonth = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    year: 2023,
    month: 2,
    day: 30,
  });

  assertEq(withMonthCode.equals(withMonth), true);
}

// Inconsistent eraYear and year are ignored when monthCode is present.
{
  let pmd = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    era: "ce",
    eraYear: 2024,
    year: 2023,
    monthCode: "M01",
    day: 1,
  });

  let expected = Temporal.PlainMonthDay.from({
    calendar: "gregory",
    monthCode: "M01",
    day: 1,
  });

  assertEq(pmd.equals(expected), true);
}

// eraYear and year must be consistent when month is present.
{
  let fields = {
    calendar: "gregory",
    era: "ce",
    eraYear: 2024,
    year: 2023,
    month: 1,
    day: 1,
  };
  assertThrowsInstanceOf(() => Temporal.PlainMonthDay.from(fields), RangeError);
}

// monthCode and month must be consistent.
{
  let fields = {
    calendar: "gregory",
    year: 2024,
    monthCode: "M01",
    month: 2,
    day: 1,
  };
  assertThrowsInstanceOf(() => Temporal.PlainMonthDay.from(fields), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
