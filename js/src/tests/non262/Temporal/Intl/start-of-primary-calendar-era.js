// |reftest| skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

const calendars = {
  iso8601: "0001-01-01",
  buddhist: "-000542-01-01",
  chinese: "0001-02-10",
  coptic: "0284-08-29",
  dangi: "0001-02-10",
  ethiopic: "0008-08-27",
  ethioaa: "-005492-07-17",
  gregory: "0001-01-01",
  hebrew: "-003760-09-07",
  indian: "0079-03-22",
  islamic: "0622-07-19",
  "islamic-civil": "0622-07-19",
  "islamic-rgsa": "0622-07-19",
  "islamic-tbla": "0622-07-18",
  "islamic-umalqura": "0622-07-19",
  japanese: "0001-01-01",
  persian: "0622-03-21",
  roc: "1912-01-01",
};

assertEqArray(
  Intl.supportedValuesOf("calendar").sort(),
  Object.keys(calendars).sort()
);

// See bug 1950425.
const calendarsNotEnabledInRelease = [
  "islamic",
  "islamic-rgsa",
  "islamic-umalqura",
];
assertEq(calendarsNotEnabledInRelease.every(c => c in calendars), true);

for (let [calendar, value] of Object.entries(calendars)) {
  if (calendarsNotEnabledInRelease.includes(calendar)) {
    continue;
  }

  // Ensure year 1 matches the expected date.
  let yearOne = Temporal.PlainDate.from({calendar, year: 1, month: 1, day: 1});
  let expected = Temporal.PlainDate.from(value).withCalendar(calendar);
  assertEq(
    Temporal.PlainDate.compare(yearOne, expected),
    0,
    yearOne.toString(),
  );

  // Ensure year 0 is equal to subtracting one year from year 1.
  let yearZero = Temporal.PlainDate.from({calendar, year: 0, month: 1, day: 1});
  assertEq(
    Temporal.PlainDate.compare(yearZero, yearOne.subtract("P1Y")),
    0,
    yearZero.toString(),
  );
}

if (typeof reportCompare === "function")
  reportCompare(true, true);

