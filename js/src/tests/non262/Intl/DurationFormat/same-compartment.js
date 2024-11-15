// |reftest| skip-if(!this.hasOwnProperty('Intl')||!Intl.hasOwnProperty("DurationFormat")||!this.wrapWithProto)

var locale = "en";
var duration = {years: 123};

var durationFormat = new Intl.DurationFormat(locale);
var scwDurationFormat = wrapWithProto(durationFormat, Intl.DurationFormat.prototype);

// Intl.DurationFormat.prototype.format
{
  var fn = Intl.DurationFormat.prototype.format;

  var expectedValue = fn.call(durationFormat, duration);
  var actualValue = fn.call(scwDurationFormat, duration);

  assertEq(actualValue, expectedValue);
}

// Intl.DurationFormat.prototype.formatToParts
{
  var fn = Intl.DurationFormat.prototype.formatToParts;

  var expectedValue = fn.call(durationFormat, duration);
  var actualValue = fn.call(scwDurationFormat, duration);

  assertDeepEq(actualValue, expectedValue);
}

// Intl.DurationFormat.prototype.resolvedOptions
{
  var fn = Intl.DurationFormat.prototype.resolvedOptions;

  var expectedValue = fn.call(durationFormat);
  var actualValue = fn.call(scwDurationFormat);

  assertDeepEq(actualValue, expectedValue);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
