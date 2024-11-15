// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat"))

const {
  Integer, Group, Decimal, Fraction, Literal, Unit
} = NumberFormatParts;

const {
  Year, Month, Week, Day, Hour, Minute, Second, Millisecond, Microsecond, Nanosecond
} = DurationFormatParts;

const tests = {
  "en": [
    {
      options: {},
      data: [
        {
          duration: {years: 1},
          expected: [
            ...Year(Integer("1"), Literal(" "), Unit("yr")),
          ],
        },
        {
          duration: {years: 2},
          expected: [
            ...Year(Integer("2"), Literal(" "), Unit("yrs")),
          ],
        },
      ],
    },
  ],
  "de": [
    {
      options: {style: "long"},
      data: [
        {
          duration: {years: 1, months: 2, weeks: 3, days: 4},
          expected: [
            ...Year(Integer("1"), Literal(" "), Unit("Jahr")),
            Literal(", "),
            ...Month(Integer("2"), Literal(" "), Unit("Monate")),
            Literal(", "),
            ...Week(Integer("3"), Literal(" "), Unit("Wochen")),
            Literal(" und "),
            ...Day(Integer("4"), Literal(" "), Unit("Tage")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("1"), Literal(" "), Unit("Stunde")),
            Literal(", "),
            ...Minute(Integer("2"), Literal(" "), Unit("Minuten")),
          ],
        },
        {
          duration: {minutes: 3, seconds: 4},
          expected: [
            ...Minute(Integer("3"), Literal(" "), Unit("Minuten")),
            Literal(", "),
            ...Second(Integer("4"), Literal(" "), Unit("Sekunden")),
          ],
        },
        {
          duration: {seconds: 5, milliseconds: 6},
          expected: [
            ...Second(Integer("5"), Literal(" "), Unit("Sekunden")),
            Literal(", "),
            ...Millisecond(Integer("6"), Literal(" "), Unit("Millisekunden")),
          ],
        },
        {
          duration: {milliseconds: 7, microseconds: 8},
          expected: [
            ...Millisecond(Integer("7"), Literal(" "), Unit("Millisekunden")),
            Literal(", "),
            ...Microsecond(Integer("8"), Literal(" "), Unit("Mikrosekunden")),
          ],
        },
        {
          duration: {microseconds: 9, nanoseconds: 10},
          expected: [
            ...Microsecond(Integer("9"), Literal(" "), Unit("Mikrosekunden")),
            Literal(", "),
            ...Nanosecond(Integer("10"), Literal(" "), Unit("Nanosekunden")),
          ],
        },

        // Gaps between time units.
        {
          duration: {hours: 1, seconds: 2},
          expected: [
            ...Hour(Integer("1"), Literal(" "), Unit("Stunde")),
            Literal(", "),
            ...Second(Integer("2"), Literal(" "), Unit("Sekunden")),
          ],
        },
        {
          duration: {hours: 3, milliseconds: 4},
          expected: [
            ...Hour(Integer("3"), Literal(" "), Unit("Stunden")),
            Literal(", "),
            ...Millisecond(Integer("4"), Literal(" "), Unit("Millisekunden")),
          ],
        },
        {
          duration: {hours: 5, microseconds: 6},
          expected: [
            ...Hour(Integer("5"), Literal(" "), Unit("Stunden")),
            Literal(", "),
            ...Microsecond(Integer("6"), Literal(" "), Unit("Mikrosekunden")),
          ],
        },
        {
          duration: {hours: 7, nanoseconds: 9},
          expected: [
            ...Hour(Integer("7"), Literal(" "), Unit("Stunden")),
            Literal(", "),
            ...Nanosecond(Integer("9"), Literal(" "), Unit("Nanosekunden")),
          ],
        },
      ],
    },
    {
      options: {style: "short"},
      data: [
        {
          duration: {years: 1, months: 2, weeks: 3, days: 4},
          expected: [
            ...Year(Integer("1"), Literal(" "), Unit("J")),
            Literal(", "),
            ...Month(Integer("2"), Literal(" "), Unit("Mon.")),
            Literal(", "),
            ...Week(Integer("3"), Literal(" "), Unit("Wo.")),
            Literal(" und "),
            ...Day(Integer("4"), Literal(" "), Unit("Tg.")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("1"), Literal(" "), Unit("Std.")),
            Literal(", "),
            ...Minute(Integer("2"), Literal(" "), Unit("Min.")),
          ],
        },
        {
          duration: {minutes: 3, seconds: 4},
          expected: [
            ...Minute(Integer("3"), Literal(" "), Unit("Min.")),
            Literal(", "),
            ...Second(Integer("4"), Literal(" "), Unit("Sek.")),
          ],
        },
        {
          duration: {seconds: 5, milliseconds: 6},
          expected: [
            ...Second(Integer("5"), Literal(" "), Unit("Sek.")),
            Literal(", "),
            ...Millisecond(Integer("6"), Literal(" "), Unit("ms")),
          ],
        },
        {
          duration: {milliseconds: 7, microseconds: 8},
          expected: [
            ...Millisecond(Integer("7"), Literal(" "), Unit("ms")),
            Literal(", "),
            ...Microsecond(Integer("8"), Literal(" "), Unit("μs")),
          ],
        },
        {
          duration: {microseconds: 9, nanoseconds: 10},
          expected: [
            ...Microsecond(Integer("9"), Literal(" "), Unit("μs")),
            Literal(", "),
            ...Nanosecond(Integer("10"), Literal(" "), Unit("ns")),
          ],
        },
      ],
    },
    {
      options: {style: "narrow"},
      data: [
        {
          duration: {years: 1, months: 2, weeks: 3, days: 4},
          expected: [
            ...Year(Integer("1"), Literal(" "), Unit("J")),
            Literal(", "),
            ...Month(Integer("2"), Literal(" "), Unit("M")),
            Literal(", "),
            ...Week(Integer("3"), Literal(" "), Unit("W")),
            Literal(" und "),
            ...Day(Integer("4"), Literal(" "), Unit("T")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("1"), Literal(" "), Unit("Std.")),
            Literal(", "),
            ...Minute(Integer("2"), Literal(" "), Unit("Min.")),
          ],
        },
        {
          duration: {minutes: 3, seconds: 4},
          expected: [
            ...Minute(Integer("3"), Literal(" "), Unit("Min.")),
            Literal(", "),
            ...Second(Integer("4"), Literal(" "), Unit("Sek.")),
          ],
        },
        {
          duration: {seconds: 5, milliseconds: 6},
          expected: [
            ...Second(Integer("5"), Literal(" "), Unit("Sek.")),
            Literal(", "),
            ...Millisecond(Integer("6"), Literal(" "), Unit("ms")),
          ],
        },
        {
          duration: {milliseconds: 7, microseconds: 8},
          expected: [
            ...Millisecond(Integer("7"), Literal(" "), Unit("ms")),
            Literal(", "),
            ...Microsecond(Integer("8"), Literal(" "), Unit("μs")),
          ],
        },
        {
          duration: {microseconds: 9, nanoseconds: 10},
          expected: [
            ...Microsecond(Integer("9"), Literal(" "), Unit("μs")),
            Literal(", "),
            ...Nanosecond(Integer("10"), Literal(" "), Unit("ns")),
          ],
        },
      ],
    },
  ],
  "fr": [
    {
      options: {style: "digital"},
      data: [
        // "digital" style defaults to "short" for non-numeric parts.
        {
          duration: {years: 111, months: 222, weeks: 333, days: 444},
          expected: [
            ...Year(Integer("111"), Literal(" "), Unit("ans")),
            Literal(", "),
            ...Month(Integer("222"), Literal(" "), Unit("m.")),
            Literal(", "),
            ...Week(Integer("333"), Literal(" "), Unit("sem.")),
            Literal(", "),
            ...Day(Integer("444"), Literal(" "), Unit("j")),
            Literal(" et "),
            ...Hour(Integer("0")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("00")),
          ],
        },

        {
          duration: {hours: 1, minutes: 0},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("00")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("00")),
          ],
        },

        // Fractional digits default to min=0 and max=9.
        {
          duration: {hours: 1, minutes: 2, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },

        // Zero unit digital parts aren't omitted.
        {
          duration: {hours: 1, minutes: 0, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },
      ],
    },
    {
      options: {style: "digital", fractionalDigits: 9},
      data: [
        {
          duration: {hours: 1, minutes: 2, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },
        {
          duration: {hours: 1, minutes: 0, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },
      ],
    },
    {
      options: {
        style: "digital",
        hoursDisplay: "always",
        minutesDisplay: "always",
        secondsDisplay: "always",
        fractionalDigits: 9,
      },
      data: [
        {
          duration: {hours: 1},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("00"), Decimal(","), Fraction("000000000")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("00"), Decimal(","), Fraction("000000000")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },
        {
          duration: {hours: 1, minutes: 0, seconds: 3, milliseconds: 4, microseconds: 5, nanoseconds: 6},
          expected: [
            ...Hour(Integer("1")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("03"), Decimal(","), Fraction("004005006")),
          ],
        },
      ],
    },
  ],
  "es": [
    {
      options: {
        seconds: "2-digit",
        fractionalDigits: 3,
      },
      data: [
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("000"))],
        },
        {
          duration: {seconds: 0, milliseconds: 1, microseconds: 2, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 1002003},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 123001002003},
          expected: [...Second(Integer("123"), Decimal(","), Fraction("001"))],
        },
      ],
    },
    {
      options: {
        seconds: "2-digit",
        fractionalDigits: 6,
      },
      data: [
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("000000"))],
        },
        {
          duration: {seconds: 0, milliseconds: 1, microseconds: 2, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001002"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 1002003},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001002"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 123001002003},
          expected: [...Second(Integer("123"), Decimal(","), Fraction("001002"))],
        },
      ],
    },
    {
      options: {
        seconds: "2-digit",
        fractionalDigits: 9,
      },
      data: [
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("000000003"))],
        },
        {
          duration: {seconds: 0, milliseconds: 1, microseconds: 2, nanoseconds: 3},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001002003"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 1002003},
          expected: [...Second(Integer("00"), Decimal(","), Fraction("001002003"))],
        },
        {
          duration: {seconds: 0, milliseconds: 0, microseconds: 0, nanoseconds: 123001002003},
          expected: [...Second(Integer("123"), Decimal(","), Fraction("001002003"))],
        },
      ],
    },
  ],
  "ar-EG": [
    {
      options: {
        years: "long",
        months: "short",
        weeks: "short",
        days: "narrow",
        yearsDisplay: "always",
        monthsDisplay: "always",
        weeksDisplay: "always",
        daysDisplay: "always",
      },
      data: [
        {
          duration: {years: 100, months: 200, weeks: 0, days: 1000},
          expected: [
            ...Year(Integer("١٠٠"), Literal(" "), Unit("سنة")),
            Literal("، و"),
            ...Month(Integer("٢٠٠"), Literal(" "), Unit("شهر")),
            Literal("، و"),
            ...Week(Integer("٠"), Literal(" "), Unit("أسبوع")),
            Literal("، و"),
            ...Day(Integer("١"), Group("٬"), Integer("٠٠٠"), Literal(" "), Unit("ي")),
          ],
        },
      ],
    }
  ],
  "zh": [
    {
      options: {
        years: "narrow",
        months: "narrow",
        weeks: "short",
        days: "short",
        yearsDisplay: "always",
        monthsDisplay: "always",
        weeksDisplay: "always",
        daysDisplay: "always",
        hoursDisplay: "always",
        minutesDisplay: "always",
        secondsDisplay: "always",
        millisecondsDisplay: "always",
        microsecondsDisplay: "always",
        nanosecondsDisplay: "always",
      },
      data: [
        {
          duration: {years: 100, months: 200, weeks: 0, days: 1000},
          expected: [
            ...Year(Integer("100"), Unit("年")),
            ...Month(Integer("200"), Unit("个月")),
            ...Week(Integer("0"), Unit("周")),
            ...Day(Integer("1"), Group(","), Integer("000"), Unit("天")),
            ...Hour(Integer("0"), Unit("小时")),
            ...Minute(Integer("0"), Unit("分钟")),
            ...Second(Integer("0"), Unit("秒")),
            ...Millisecond(Integer("0"), Unit("毫秒")),
            ...Microsecond(Integer("0"), Unit("微秒")),
            ...Nanosecond(Integer("0"), Unit("纳秒")),
          ],
        },
        {
          duration: {years: 0},
          expected: [
            ...Year(Integer("0"), Unit("年")),
            ...Month(Integer("0"), Unit("个月")),
            ...Week(Integer("0"), Unit("周")),
            ...Day(Integer("0"), Unit("天")),
            ...Hour(Integer("0"), Unit("小时")),
            ...Minute(Integer("0"), Unit("分钟")),
            ...Second(Integer("0"), Unit("秒")),
            ...Millisecond(Integer("0"), Unit("毫秒")),
            ...Microsecond(Integer("0"), Unit("微秒")),
            ...Nanosecond(Integer("0"), Unit("纳秒")),
          ],
        },
        {
          duration: {nanoseconds: 0},
          expected: [
            ...Year(Integer("0"), Unit("年")),
            ...Month(Integer("0"), Unit("个月")),
            ...Week(Integer("0"), Unit("周")),
            ...Day(Integer("0"), Unit("天")),
            ...Hour(Integer("0"), Unit("小时")),
            ...Minute(Integer("0"), Unit("分钟")),
            ...Second(Integer("0"), Unit("秒")),
            ...Millisecond(Integer("0"), Unit("毫秒")),
            ...Microsecond(Integer("0"), Unit("微秒")),
            ...Nanosecond(Integer("0"), Unit("纳秒")),
          ],
        },
      ],
    }
  ],
};

for (let [locale, list] of Object.entries(tests)) {
  for (let {options, data} of list) {
    let df = new Intl.DurationFormat(locale, options);
    for (let {duration, expected} of data) {
      let str = PartsToString(expected);

      assertEq(df.format(duration), str,
               `${locale} [${JSON.stringify(options)}]: ${JSON.stringify(duration)}`);

      let parts = df.formatToParts(duration);
      assertEq(PartsToString(parts), str,
               `${locale} [${JSON.stringify(options)}]: ${JSON.stringify(duration)}`);

      assertEq(parts.length, expected.length,
               `${locale} [${JSON.stringify(options)}]: ${JSON.stringify(duration)}`);

      assertDeepEq(parts, expected);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
