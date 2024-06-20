// |reftest| skip-if(!this.hasOwnProperty("Temporal"))

// Common leap months should find a result not too far into the past.
//
// Month -> ISO year
// 
// M01L     <uncommon>
// M02L     1765
// M03L     1955
// M04L     1944
// M05L     1952
// M06L     1941
// M07L     1938
// M08L     1718
// M09L     <uncommon>
// M10L     <uncommon>
// M11L     <uncommon>
// M12L     <uncommon>
//
// M02L and M08L with 29 days is common, but with 30 is actually rather uncommon.
//
// See also "The Mathematics of the Chinese Calendar", Table 21 [1] for a
// distribution of leap months.
//
// [1] https://www.xirugu.com/CHI500/Dates_Time/Chinesecalender.pdf

const monthCodes = [
  // M01L is an uncommon leap month.
  "M02L",
  "M03L",
  "M04L",
  "M05L",
  "M06L",
  "M07L",
  "M08L",
  // M09L..M12L are uncommon leap months.
];

function assertSameISOFields(actual, expected) {
  let actualFields = actual.getISOFields();
  let expectedFields = expected.getISOFields();

  assertEq(typeof actualFields.isoYear, "number");
  assertEq(typeof actualFields.isoMonth, "number");
  assertEq(typeof actualFields.isoDay, "number");

  assertEq(actualFields.isoMonth > 0, true);
  assertEq(actualFields.isoDay > 0, true);

  assertEq(actualFields.isoYear, expectedFields.isoYear);
  assertEq(actualFields.isoMonth, expectedFields.isoMonth);
  assertEq(actualFields.isoDay, expectedFields.isoDay);
}

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
