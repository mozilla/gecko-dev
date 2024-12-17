// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal"))

// Non-leap month should find a result in years around 1972.
//
// Month -> ISO year
// 
// M01      1970
// M02      1972
// M03      1966
// M04      1970
// M05      1972
// M06      1971
// M07      1972
// M08      1971
// M09      1972
// M10      1972
// M11      1970
// M12      1972

const monthCodes = [
  "M01",
  "M02",
  "M03",
  "M04",
  "M05",
  "M06",
  "M07",
  "M08",
  "M09",
  "M10",
  "M11",
  "M12",
];

const calendar = "chinese";

// Months can have up to 30 days.
const day = 30;

for (let monthCode of monthCodes) {
  let pmd = Temporal.PlainMonthDay.from({calendar, monthCode, day});
  assertEq(pmd.monthCode, monthCode);
  assertEq(pmd.day, day);

  let constrain = Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "constrain"});
  assertEq(constrain.monthCode, monthCode);
  assertEq(constrain.day, day);
  assertSameISOFields(constrain, pmd);

  assertThrowsInstanceOf(() => {
    Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "reject"});
  }, RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
