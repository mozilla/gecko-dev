// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat"))

const {
  Integer, Literal, Unit
} = NumberFormatParts;

const {
  Hour, Minute, Second
} = DurationFormatParts;

const tests = {
  "en": [
    {
      options: {
        hours: "2-digit",
      },
      data: [
        {
          duration: {hours: 1, minutes: 0},
          expected: [
            ...Hour(Integer("01")),
            Literal(":"),
            ...Minute(Integer("00")),
            Literal(":"),
            ...Second(Integer("00")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("01")),
            Literal(":"),
            ...Minute(Integer("02")),
            Literal(":"),
            ...Second(Integer("00")),
          ],
        },
      ],
    },
  ],
  "da": [
    {
      options: {
        hours: "2-digit",
      },
      data: [
        {
          duration: {hours: 1, minutes: 0},
          expected: [
            ...Hour(Integer("01")),
            Literal("."),
            ...Minute(Integer("00")),
            Literal("."),
            ...Second(Integer("00")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("01")),
            Literal("."),
            ...Minute(Integer("02")),
            Literal("."),
            ...Second(Integer("00")),
          ],
        },
      ],
    },
  ],
  "da-u-nu-arabext": [
    {
      options: {
        hours: "2-digit",
      },
      data: [
        {
          duration: {hours: 1, minutes: 0},
          expected: [
            ...Hour(Integer("۰۱")),
            Literal("٫"),
            ...Minute(Integer("۰۰")),
            Literal("٫"),
            ...Second(Integer("۰۰")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("۰۱")),
            Literal("٫"),
            ...Minute(Integer("۰۲")),
            Literal("٫"),
            ...Second(Integer("۰۰")),
          ],
        },
      ],
    },
  ],
  "ur-IN": [
    {
      options: {
        hours: "2-digit",
      },
      data: [
        {
          duration: {hours: 1, minutes: 0},
          expected: [
            ...Hour(Integer("۰۱")),
            Literal("٫"),
            ...Minute(Integer("۰۰")),
            Literal("٫"),
            ...Second(Integer("۰۰")),
          ],
        },
        {
          duration: {hours: 1, minutes: 2},
          expected: [
            ...Hour(Integer("۰۱")),
            Literal("٫"),
            ...Minute(Integer("۰۲")),
            Literal("٫"),
            ...Second(Integer("۰۰")),
          ],
        },
      ],
    },
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
