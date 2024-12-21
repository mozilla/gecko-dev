// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test formatting for Temporal.MonthDay.

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
    month: "long",
  },
  {
    month: "numeric",
    day: "numeric",
  },
];

const dateStyles = [
  "full", "long", "medium", "short"
];

// The current implementation computes the skeleton from the resolved pattern,
// which doesn't always produce the correct results. It'd be better to compute
// the skeleton from ICU's "DateTimeSkeletons" resource table. This requires
// writing manual resource lookup code, though. And "DateTimeSkeletons" doesn't
// (yet) contain the correct skeletons in all cases. For more info see these
// ICU and CLDR bugs:
// - https://unicode-org.atlassian.net/browse/ICU-22867
// - https://unicode-org.atlassian.net/browse/CLDR-14993
// - https://unicode-org.atlassian.net/browse/CLDR-18136

const dateStyleToOptions = {
  full: {
    month: "long",
    day: "numeric",
  },
  long: {
    month: "long",
    day: "numeric",
  },
  medium: {
    month: "short",
    day: "numeric",
  },
  short: {
    month: "numeric",
    day: "numeric",
  },
  overrides: {
    de: {
      medium: {
        month: "2-digit",
        day: "2-digit",
      },
      short: {
        month: "2-digit",
        day: "2-digit",
      },
    },
    ar: {
      medium: {
        month: "2-digit",
        day: "2-digit",
      },
    },
    zh: {
      full: {
        month: "numeric",
        day: "numeric",
      },
      long: {
        month: "numeric",
        day: "numeric",
      },
      medium: {
        month: "numeric",
        day: "numeric",
      },
    },
    ja: {
      full: {
        month: "numeric",
        day: "numeric",
      },
      long: {
        month: "numeric",
        day: "numeric",
      },
      medium: {
        month: "2-digit",
        day: "2-digit",
      },
      short: {
        month: "2-digit",
        day: "2-digit",
      },
    },
  },
};

const timeZone = "UTC";

let date = new Date(0);
let instant = date.toTemporalInstant();
let zonedDateTime = instant.toZonedDateTimeISO(timeZone);
let plainDate = zonedDateTime.toPlainDate();

for (let locale of locales) {
  // Get the default calendar for |locale|.
  let calendar = new Intl.DateTimeFormat(locale).resolvedOptions().calendar;

  // Calendar must match for MonthDay.
  //
  // https://github.com/js-temporal/proposal-temporal-v2/issues/29
  let plainMonthDay = plainDate.withCalendar(calendar).toPlainMonthDay();

  for (let opts of options) {
    let expected = date.toLocaleDateString(locale, {timeZone, ...opts});
    assertEq(plainMonthDay.toLocaleString(locale, {timeZone, ...opts}), expected);
  }

  for (let dateStyle of dateStyles) {
    let opts = dateStyleToOptions.overrides[locale]?.[dateStyle] ?? dateStyleToOptions[dateStyle];
    assertEq(
      plainMonthDay.toLocaleString(locale, {timeZone, dateStyle}),
      date.toLocaleDateString(locale, {timeZone, ...opts}),
      `l=${locale}, s=${dateStyle}, o=${JSON.stringify(opts)}`
    );
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
