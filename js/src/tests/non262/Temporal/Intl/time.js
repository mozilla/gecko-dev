// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test Temporal types which can produce time formats.

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
  {
    hour: "2-digit",
    minute: "numeric",
    second: "numeric",
    fractionalSecondDigits: 3,
  },
  {
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
  },
  {
    hour: "numeric",
    minute: "numeric",
  },

  {
    dayPeriod: "long",
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
    fractionalSecondDigits: 3,
  },
  {
    dayPeriod: "long",
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
  },
  {
    dayPeriod: "short",
    hour: "2-digit",
    minute: "numeric",
  },
  {
    dayPeriod: "narrow",
    hour: "numeric",
  },

  {
    minute: "2-digit",
    second: "2-digit",
  },
];

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDateTime = zonedDateTime.toPlainDateTime();
let plainTime = zonedDateTime.toPlainTime();

for (let locale of locales) {
  let expected = date.toLocaleTimeString(locale, {timeZone});
  assertEq(plainTime.toLocaleString(locale, {timeZone}), expected);

  for (let opts of options) {
    let expected = date.toLocaleTimeString(locale, {timeZone, ...opts});
    assertEq(instant.toLocaleString(locale, {timeZone, ...opts}), expected);
    assertEq(zonedDateTime.toLocaleString(locale, {...opts}), expected);
    assertEq(plainDateTime.toLocaleString(locale, {timeZone, ...opts}), expected);
    assertEq(plainTime.toLocaleString(locale, {timeZone, ...opts}), expected);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
