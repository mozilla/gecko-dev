// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty("Temporal")||!this.hasOwnProperty("Intl"))

// Test formatting for Temporal.YearMonth.

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
    year: "numeric",
    month: "numeric",
  },
  {
    month: "long",
  },

  {
    era: "long",
    year: "numeric",
  },
  {
    era: "short",
    year: "2-digit",
    month: "narrow",
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
    year: "numeric",
    month: "long",
  },
  long: {
    year: "numeric",
    month: "long",
  },
  medium: {
    year: "numeric",
    month: "short",
  },
  short: {
    year: "2-digit",
    month: "numeric",
  },
  overrides: {
    de: {
      medium: {
        year: "numeric",
        month: "2-digit",
      },
      short: {
        year: "2-digit",
        month: "2-digit",
      },
    },
    fr: {
      short: {
        year: "numeric",
        month: "2-digit",
      },
    },
    ar: {
      medium: {
        year: "numeric",
        month: "2-digit",
      },
      short: {
        year: "numeric",
        month: "numeric",
      },
    },
    th: {
      full: {
        era: "short",
        year: "numeric",
        month: "long",
      },
    },
    zh: {
      full: {
        year: "numeric",
        month: "numeric",
      },
      long: {
        year: "numeric",
        month: "numeric",
      },
      medium: {
        year: "numeric",
        month: "numeric",
      },
      short: {
        year: "numeric",
        month: "numeric",
      },
    },
    ja: {
      full: {
        year: "numeric",
        month: "numeric",
      },
      long: {
        year: "numeric",
        month: "numeric",
      },
      medium: {
        year: "numeric",
        month: "2-digit",
      },
      short: {
        year: "numeric",
        month: "2-digit",
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

  // Calendar must match for YearMonth.
  //
  // https://github.com/js-temporal/proposal-temporal-v2/issues/29
  let plainYearMonth = plainDate.withCalendar(calendar).toPlainYearMonth();

  for (let opts of options) {
    assertEq(
      plainYearMonth.toLocaleString(locale, {timeZone, ...opts}),
      date.toLocaleDateString(locale, {timeZone, ...opts})
    );
  }

  for (let dateStyle of dateStyles) {
    let opts = dateStyleToOptions.overrides[locale]?.[dateStyle] ?? dateStyleToOptions[dateStyle];
    assertEq(
      plainYearMonth.toLocaleString(locale, {timeZone, dateStyle}),
      date.toLocaleDateString(locale, {timeZone, ...opts})
    );
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
