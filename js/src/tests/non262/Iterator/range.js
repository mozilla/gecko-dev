// |reftest| shell-option(--enable-iterator-range) skip-if(!Iterator.hasOwnProperty('range'))

// Invalid start parameter types
assertThrowsInstanceOf(() => Iterator.range('1'), TypeError);
assertThrowsInstanceOf(() => Iterator.range(null), TypeError);
assertThrowsInstanceOf(() => Iterator.range(undefined), TypeError);
assertThrowsInstanceOf(() => Iterator.range({}), TypeError);
assertThrowsInstanceOf(() => Iterator.range([]), TypeError);
assertThrowsInstanceOf(() => Iterator.range(true), TypeError);
assertThrowsInstanceOf(() => Iterator.range(Symbol()), TypeError);

// Invalid end parameter types
assertThrowsInstanceOf(() => Iterator.range(0, '1'), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, null), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, undefined), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, {}), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, []), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, true), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, Symbol()), TypeError);

// Invalid step parameter types
assertThrowsInstanceOf(() => Iterator.range(0, 10, '1'), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, 10, true), TypeError);
assertThrowsInstanceOf(() => Iterator.range(0, 10, Symbol()), TypeError);

// NaN and Infinity tests
assertThrowsInstanceOf(() => Iterator.range(NaN), RangeError);
assertThrowsInstanceOf(() => Iterator.range(0, NaN), RangeError);
assertThrowsInstanceOf(() => Iterator.range(Infinity), TypeError);

// Step type and value tests
assertThrowsInstanceOf(() => Iterator.range(0, 10, NaN), RangeError);
assertThrowsInstanceOf(() => Iterator.range(0, 10, Infinity), RangeError);

// Zero step tests
assertThrowsInstanceOf(() => Iterator.range(0, 10, 0), RangeError);
Iterator.range(0, 0, 0);

// Step configuration object tests
Iterator.range(0, 10, { step: 2 });
Iterator.range(0, 10, { step: -1 });
Iterator.range(0, 10, { inclusiveEnd: true });
assertThrowsInstanceOf(() => Iterator.range(0, 10, { step: '2' }), TypeError);

// Basic number inputs
Iterator.range(0, 10);
Iterator.range(0, 10, 2);


// Basic sequences with increasing steps of 1
const result1 = Array.from(Iterator.range(0, 10));
assertDeepEq(result1, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);

const result2 = Array.from(Iterator.range(2, 6));
assertDeepEq(result2, [2, 3, 4, 5]);

// Test empty range
const result3 = Array.from(Iterator.range(0, 0));
assertDeepEq(result3, []);

const result4 = Array.from(Iterator.range(5, 5));
assertDeepEq(result4, []);

//TODO: support/test other sequences

if (typeof reportCompare === 'function') {
    reportCompare(0, 0);
}
