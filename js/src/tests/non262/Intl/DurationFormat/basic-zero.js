// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat"))

// Test formatting zero amount for a single unit and compare "auto" against
// "always" for the display option.

const {
  Integer, Literal, Unit
} = NumberFormatParts;

function ToDurationFormatPart(unit) {
  unit = unit.slice(0, -1);
  unit = unit[0].toUpperCase() + unit.slice(1);
  return DurationFormatParts[unit];
}

const tests = {
  "en": Object.fromEntries(units.map(unit => [unit, unit])),
  "de": {
    "years": "Jahre",
    "months": "Monate",
    "weeks": "Wochen",
    "days": "Tage",
    "hours": "Stunden",
    "minutes": "Minuten",
    "seconds": "Sekunden",
    "milliseconds": "Millisekunden",
    "microseconds": "Mikrosekunden",
    "nanoseconds": "Nanosekunden",
  },
};

for (let [locale, data] of Object.entries(tests)) {
  for (let unit of units) {
    let auto = new Intl.DurationFormat(locale, {
      style: "long",
      [unit + "Display"]: "auto",
    });
    let always = new Intl.DurationFormat(locale, {
      style: "long",
      [unit + "Display"]: "always",
    });

    let duration = {[unit]: 0};
    let expected = `0 ${data[unit]}`;

    // Empty string.
    assertEq(auto.format(duration), "", `auto: ${unit}`);

    // Empty array.
    assertEq(auto.formatToParts(duration).length, 0, `auto: ${unit}`);

    assertEq(always.format(duration), expected, `always: ${unit}`);

    let parts = always.formatToParts(duration);
    assertEq(PartsToString(parts), expected, `always: ${unit}`);

    assertDeepEq(parts, [
      ...ToDurationFormatPart(unit)(Integer("0"), Literal(" "), Unit(data[unit])),
    ]);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
