// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat"))

var g = newGlobal();

var locale = "en";
var duration = {years: 123};

var durationFormat = new Intl.DurationFormat(locale);
var ccwDurationFormat = new g.Intl.DurationFormat(locale);

// Intl.DurationFormat.prototype.format
{
  var fn = Intl.DurationFormat.prototype.format;

  var expectedValue = fn.call(durationFormat, duration);
  var actualValue = fn.call(ccwDurationFormat, duration);

  assertEq(actualValue, expectedValue);
}

// Intl.DurationFormat.prototype.formatToParts
{
  var fn = Intl.DurationFormat.prototype.formatToParts;

  var expectedValue = fn.call(durationFormat, duration);
  var actualValue = fn.call(ccwDurationFormat, duration);

  assertDeepEq(actualValue, expectedValue);
}

// Intl.DurationFormat.prototype.resolvedOptions
{
  var fn = Intl.DurationFormat.prototype.resolvedOptions;

  var expectedValue = fn.call(durationFormat);
  var actualValue = fn.call(ccwDurationFormat);

  assertDeepEq(actualValue, expectedValue);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
