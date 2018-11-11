// |reftest| skip-if(!this.hasOwnProperty("Intl"))
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Tests the format function with a diverse set of locales and options.
// Always use UTC to avoid dependencies on test environment.

/*
 * Return true if A is equal to B, where equality on arrays and objects
 * means that they have the same set of enumerable properties, the values
 * of each property are deep_equal, and their 'length' properties are
 * equal. Equality on other types is ==.
 */
function deepEqual(a, b) {
    if (typeof a !== typeof b)
        return false;

    if (a === null)
        return b === null;

    if (typeof a === 'object') {
        // For every property of a, does b have that property with an equal value?
        var props = {};
        for (var prop in a) {
            if (!deepEqual(a[prop], b[prop]))
                return false;
            props[prop] = true;
        }

        // Are all of b's properties present on a?
        for (var prop in b)
            if (!props[prop])
                return false;

        // length isn't enumerable, but we want to check it, too.
        return a.length === b.length;
    }

    return Object.is(a, b);
}

function composeDate(parts) {
  return parts.map(({value}) => value)
              .reduce((string, part) => string + part);
}

var format;
var date = Date.UTC(2012, 11, 17, 3, 0, 42);

// Locale en-US; default options.
format = new Intl.DateTimeFormat("en-us", {timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'month', value: '12' },
  { type: 'literal', value: '/' },
  { type: 'day', value: '17' },
  { type: 'literal', value: '/' },
  { type: 'year', value: '2012' }
]), true);

// Just date
format = new Intl.DateTimeFormat("en-us", {
  year: 'numeric',
  month: 'numeric',
  day: 'numeric',
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'month', value: '12' },
  { type: 'literal', value: '/' },
  { type: 'day', value: '17' },
  { type: 'literal', value: '/' },
  { type: 'year', value: '2012' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Just time in hour24
format = new Intl.DateTimeFormat("en-us", {
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: false,
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'hour', value: '03' },
  { type: 'literal', value: ':' },
  { type: 'minute', value: '00' },
  { type: 'literal', value: ':' },
  { type: 'second', value: '42' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Just time in hour12
format = new Intl.DateTimeFormat("en-us", {
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: true,
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'hour', value: '3' },
  { type: 'literal', value: ':' },
  { type: 'minute', value: '00' },
  { type: 'literal', value: ':' },
  { type: 'second', value: '42' },
  { type: 'literal', value: ' ' },
  { type: 'dayPeriod', value: 'AM' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Just month.
format = new Intl.DateTimeFormat("en-us", {
  month: "narrow",
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'month', value: 'D' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Just weekday.
format = new Intl.DateTimeFormat("en-us", {
  weekday: "narrow",
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'weekday', value: 'M' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Year and era.
format = new Intl.DateTimeFormat("en-us", {
  year: "numeric",
  era: "short",
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'year', value: '2012' },
  { type: 'literal', value: ' ' },
  { type: 'era', value: 'AD' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

// Time and date
format = new Intl.DateTimeFormat("en-us", {
  weekday: 'long',
  year: 'numeric',
  month: 'numeric',
  day: 'numeric',
  hour: 'numeric',
  minute: 'numeric',
  second: 'numeric',
  hour12: true,
  timeZone: "UTC"});
assertEq(deepEqual(format.formatToParts(date), [
  { type: 'weekday', value: 'Monday' },
  { type: 'literal', value: ', ' },
  { type: 'month', value: '12' },
  { type: 'literal', value: '/' },
  { type: 'day', value: '17' },
  { type: 'literal', value: '/' },
  { type: 'year', value: '2012' },
  { type: 'literal', value: ', ' },
  { type: 'hour', value: '3' },
  { type: 'literal', value: ':' },
  { type: 'minute', value: '00' },
  { type: 'literal', value: ':' },
  { type: 'second', value: '42' },
  { type: 'literal', value: ' ' },
  { type: 'dayPeriod', value: 'AM' }
]), true);
assertEq(composeDate(format.formatToParts(date)), format.format(date));

if (typeof reportCompare === "function")
    reportCompare(0, 0, 'ok');
