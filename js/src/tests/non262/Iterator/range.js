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

if (typeof reportCompare === 'function') {
    reportCompare(0, 0);
}
