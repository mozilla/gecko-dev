// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test default formatting for Temporal types using different locales and all
// supported calendars.

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

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDateTime = zonedDateTime.toPlainDateTime();
let plainDate = zonedDateTime.toPlainDate();
let plainTime = zonedDateTime.toPlainTime();

for (let locale of locales) {
  for (let calendar of Intl.supportedValuesOf("calendar")) {
    // Calendar must match for YearMonth and MonthDay.
    //
    // https://github.com/js-temporal/proposal-temporal-v2/issues/29
    let calendarDate = plainDate.withCalendar(calendar);
    let calendarYearMonth = calendarDate.toPlainYearMonth();
    let calendarMonthDay = calendarDate.toPlainMonthDay();

    assertEq(
      instant.toLocaleString(locale, {timeZone, calendar}),
      date.toLocaleString(locale, {timeZone, calendar})
    );
    assertEq(
      zonedDateTime.toLocaleString(locale, {calendar}),
      date.toLocaleString(locale, {timeZone, calendar, timeZoneName: "short"})
    );
    assertEq(
      plainDateTime.toLocaleString(locale, {timeZone, calendar}),
      date.toLocaleString(locale, {timeZone, calendar})
    );
    assertEq(
      plainDate.toLocaleString(locale, {timeZone, calendar}),
      date.toLocaleDateString(locale, {timeZone, calendar})
    );
    assertEq(
      plainTime.toLocaleString(locale, {timeZone, calendar}),
      date.toLocaleTimeString(locale, {timeZone, calendar})
    );
    assertEq(
      calendarYearMonth.toLocaleString(locale, {timeZone, calendar}),
      date.toLocaleDateString(locale, {timeZone, calendar, year: "numeric", month: "numeric"})
    );

    // ICU4X and ICU4C don't agree on calendar computations for islamic-umalqura.
    //
    // See <https://github.com/unicode-org/icu4x/issues/4982>.
    //
    // ICU4X and ICU4C are possibly both wrong for dates around 1970 when
    // comparing to these comparison charts:
    // https://web.archive.org/web/20150324181645fw_/http://www.staff.science.uu.nl/~gent0113/islam/downloads/ksa_calendar_1356_1411.pdf
    //
    // Also see:
    // https://web.archive.org/web/20110611040922if_/http://www.staff.science.uu.nl:80/~gent0113/islam/ummalqura.htm
    if (calendar !== "islamic-umalqura") {
      assertEq(
        calendarMonthDay.toLocaleString(locale, {timeZone, calendar}),
        date.toLocaleDateString(locale, {timeZone, calendar, month: "numeric", day: "numeric"}),
      );
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
