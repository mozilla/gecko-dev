// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test Temporal types which can produce date formats.

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
    weekday: "long",
    year: "numeric",
    month: "long",
    day: "numeric",
  },
  {
    year: "numeric",
    month: "short",
    day: "numeric",
  },
  {
    year: "2-digit",
    month: "narrow",
  },
  {
    month: "2-digit",
    day: "2-digit",
  },

  {
    era: "long",
    year: "numeric",
  },
  {
    era: "short",
    year: "numeric",
    month: "numeric",
  },
];

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDateTime = zonedDateTime.toPlainDateTime();
let plainDate = zonedDateTime.toPlainDate();

for (let locale of locales) {
  let expected = date.toLocaleDateString(locale, {timeZone});
  assertEq(plainDate.toLocaleString(locale, {timeZone}), expected);

  for (let opts of options) {
    let expected = date.toLocaleDateString(locale, {timeZone, ...opts});
    assertEq(instant.toLocaleString(locale, {timeZone, ...opts}), expected);
    assertEq(zonedDateTime.toLocaleString(locale, {...opts}), expected);
    assertEq(plainDateTime.toLocaleString(locale, {timeZone, ...opts}), expected);
    assertEq(plainDate.toLocaleString(locale, {timeZone, ...opts}), expected);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
