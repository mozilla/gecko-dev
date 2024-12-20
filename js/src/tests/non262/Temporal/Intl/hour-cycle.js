// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test hour cycle options (hour12 and hourCycle) are correctly set when
// formatting Temporal types.

const locales = [
  "en",
  "de",
  "fr",
  "es",
  "ar",
  "th",
  "zh",
  "ja",
];

const options = [
  {hour12: true},
  {hour12: false},
  {hourCycle: "h11"},
  {hourCycle: "h12"},
  {hourCycle: "h23"},
  {hourCycle: "h24"},
];

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDateTime = zonedDateTime.toPlainDateTime();
let plainTime = zonedDateTime.toPlainTime();

for (let locale of locales) {
  for (let opts of options) {
    assertEq(
      instant.toLocaleString(locale, {timeZone, ...opts}),
      date.toLocaleString(locale, {timeZone, ...opts})
    );
    assertEq(
      zonedDateTime.toLocaleString(locale, {...opts}),
      date.toLocaleString(locale, {timeZone, timeZoneName: "short", ...opts})
    );
    assertEq(
      plainDateTime.toLocaleString(locale, {timeZone, ...opts}),
      date.toLocaleString(locale, {timeZone, ...opts})
    );
    assertEq(
      plainTime.toLocaleString(locale, {timeZone, ...opts}),
      date.toLocaleTimeString(locale, {timeZone, ...opts})
    );

    let dtf = new Intl.DateTimeFormat(locale, {
      hour: "2-digit",
      minute: "2-digit",
      timeZone,
      ...opts,
    });

    assertEq(dtf.format(instant), dtf.format(date));
    assertEq(dtf.format(plainDateTime), dtf.format(date));
    assertEq(dtf.format(plainTime), dtf.format(date));
    assertThrowsInstanceOf(() => dtf.format(zonedDateTime), TypeError);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
