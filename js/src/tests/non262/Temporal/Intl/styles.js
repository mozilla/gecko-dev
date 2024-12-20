// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test "dateStyle" and "timeStyle" with Temporal types.

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

const dateStyles = [
  "full", "long", "medium", "short"
];

const timeStyles = [
  "full", "long", "medium", "short"
];

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDateTime = zonedDateTime.toPlainDateTime();
let plainDate = zonedDateTime.toPlainDate();
let plainTime = zonedDateTime.toPlainTime();

// let calendarDate = plainDate.withCalendar(calendar);
// let calendarYearMonth = calendarDate.toPlainYearMonth();
// let calendarMonthDay = calendarDate.toPlainMonthDay();

function assertNe(actual, expected) {
  assertEq(Object.is(actual, expected), false);
}

for (let locale of locales) {
  for (let dateStyle of dateStyles) {
    let expected = date.toLocaleDateString(locale, {timeZone, dateStyle});

    assertEq(instant.toLocaleString(locale, {timeZone, dateStyle}), expected);
    assertEq(zonedDateTime.toLocaleString(locale, {dateStyle}), expected);

    // https://github.com/tc39/proposal-temporal/issues/3062
    if (
      (locale === "zh" && (dateStyle === "long" || dateStyle === "medium")) ||
      (locale === "ja" && (dateStyle === "full" || dateStyle === "long"))
    ) {
      assertNe(plainDateTime.toLocaleString(locale, {timeZone, dateStyle}), expected);
      assertNe(plainDate.toLocaleString(locale, {timeZone, dateStyle}), expected);
      continue;
    }

    assertEq(plainDateTime.toLocaleString(locale, {timeZone, dateStyle}), expected);
    assertEq(plainDate.toLocaleString(locale, {timeZone, dateStyle}), expected);
  }

  for (let timeStyle of timeStyles) {
    let expected = date.toLocaleTimeString(locale, {timeZone, timeStyle});
    assertEq(instant.toLocaleString(locale, {timeZone, timeStyle}), expected);
    assertEq(zonedDateTime.toLocaleString(locale, {timeStyle}), expected);

    // "full" and "long" time style include time zone names, which aren't used
    // by Temporal.PlainDateTime and Temporal.PlainTime.
    if (timeStyle !== "full" && timeStyle !== "long") {
      assertEq(plainDateTime.toLocaleString(locale, {timeZone, timeStyle}), expected);
      assertEq(plainTime.toLocaleString(locale, {timeZone, timeStyle}), expected);
    }
  }

  for (let dateStyle of dateStyles) {
    for (let timeStyle of timeStyles) {
      let expected = date.toLocaleString(locale, {timeZone, dateStyle, timeStyle});

      assertEq(instant.toLocaleString(locale, {timeZone, dateStyle, timeStyle}), expected);
      assertEq(zonedDateTime.toLocaleString(locale, {dateStyle, timeStyle}), expected);

      // "full" and "long" time style include time zone names, which aren't used
      // by Temporal.PlainDateTime and Temporal.PlainTime.
      if (timeStyle !== "full" && timeStyle !== "long") {
        // https://github.com/tc39/proposal-temporal/issues/3062
        if (
          (locale === "zh" && (dateStyle === "long" || dateStyle === "medium")) ||
          (locale === "ja" && (dateStyle === "full" || dateStyle === "long"))
        ) {
          assertNe(plainDateTime.toLocaleString(locale, {timeZone, dateStyle, timeStyle}), expected);
          continue;
        }
        assertEq(plainDateTime.toLocaleString(locale, {timeZone, dateStyle, timeStyle}), expected);
      }
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
