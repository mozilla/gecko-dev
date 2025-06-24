// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test using "era" option without "year" for all possible calendars. The exact
// formatted result is not tested, it should only be consistent with the
// resolved options.

const options = [
  {
    era: "narrow",
    month: "narrow",
  },
  {
    era: "short",
    day: "numeric",
  },
  {
    era: "long",
    hour: "2-digit",
  },
];

for (let calendar of Intl.supportedValuesOf("calendar")) {
  for (let opts of options) {
    let dtf = new Intl.DateTimeFormat("en", opts);
    let resolved = dtf.resolvedOptions();
    let parts = dtf.formatToParts(0);

    // Ensure there's an "era" part when the resolved options include "era".
    assertEq(
      parts.findIndex(p => p.type === "era") >= 0,
      Object.hasOwn(resolved, "era")
    );
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
