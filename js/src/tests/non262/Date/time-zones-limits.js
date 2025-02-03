// |reftest| skip-if(winWidget||!this.hasOwnProperty("Intl")) -- Windows doesn't accept IANA names for the TZ env variable; Requires ICU time zone support

// Create dates at the minimum and maximum allowed time values. Ensure the same
// result is returned if the input is a UTC value and if the inputs are individual
// date-time parts.

const MinTime = -8640000000000000;
const MaxTime = 8640000000000000;

function dateFromParts(dt) {
  return new Date(
    dt.getFullYear(),
    dt.getMonth(),
    dt.getDate(),
    dt.getHours(),
    dt.getMinutes(),
    dt.getSeconds(),
    dt.getMilliseconds()
  );
}

inTimeZone("America/Los_Angeles", () => {
  let dtMin = new Date(MinTime);
  assertDateTime(dtMin, "Mon Apr 19 -271821 16:07:02 GMT-0752 (Pacific Standard Time)");
  assertEq(dtMin.getTimezoneOffset(), 472.96666666666664);
  assertEq(dtMin.getTime(), MinTime);

  let dtMin2 = dateFromParts(dtMin);
  assertDateTime(dtMin2, "Mon Apr 19 -271821 16:07:02 GMT-0752 (Pacific Standard Time)");
  assertEq(dtMin2.getTimezoneOffset(), 472.96666666666664);
  assertEq(dtMin2.getTime(), MinTime);

  let dtMax = new Date(MaxTime);
  assertDateTime(dtMax, "Fri Sep 12 275760 17:00:00 GMT-0700 (Pacific Daylight Time)");
  assertEq(dtMax.getTimezoneOffset(), 420);
  assertEq(dtMax.getTime(), MaxTime);

  let dtMax2 = dateFromParts(dtMax);
  assertDateTime(dtMax2, "Fri Sep 12 275760 17:00:00 GMT-0700 (Pacific Daylight Time)");
  assertEq(dtMax2.getTimezoneOffset(), 420);
  assertEq(dtMax2.getTime(), MaxTime);
});

inTimeZone("Europe/Berlin", () => {
  let dtMin = new Date(MinTime);
  assertDateTime(dtMin, "Tue Apr 20 -271821 00:53:28 GMT+0053 (Central European Standard Time)");
  assertEq(dtMin.getTimezoneOffset(), -53.46666666666667);
  assertEq(dtMin.getTime(), MinTime);

  let dtMin2 = dateFromParts(dtMin);
  assertDateTime(dtMin2, "Tue Apr 20 -271821 00:53:28 GMT+0053 (Central European Standard Time)");
  assertEq(dtMin2.getTimezoneOffset(), -53.46666666666667);
  assertEq(dtMin2.getTime(), MinTime);

  let dtMax = new Date(MaxTime);
  assertDateTime(dtMax, "Sat Sep 13 275760 02:00:00 GMT+0200 (Central European Summer Time)");
  assertEq(dtMax.getTimezoneOffset(), -120);
  assertEq(dtMax.getTime(), MaxTime);

  let dtMax2 = dateFromParts(dtMax);
  assertDateTime(dtMax2, "Sat Sep 13 275760 02:00:00 GMT+0200 (Central European Summer Time)");
  assertEq(dtMax2.getTimezoneOffset(), -120);
  assertEq(dtMax2.getTime(), MaxTime);
});

if (typeof reportCompare === "function")
  reportCompare(true, true);
