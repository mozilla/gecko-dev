// |reftest| shell-option(--enable-iterator-range) skip-if(!Iterator.hasOwnProperty('range'))

// Test Iterator.range error cases for non-number/non-bigint inputs

// Invalid start parameter types
assertThrowsInstanceOf(() => Iterator.range('1'), TypeError);
assertThrowsInstanceOf(() => Iterator.range(null), TypeError);
assertThrowsInstanceOf(() => Iterator.range(undefined), TypeError);
assertThrowsInstanceOf(() => Iterator.range({}), TypeError);
assertThrowsInstanceOf(() => Iterator.range([]), TypeError);
assertThrowsInstanceOf(() => Iterator.range(true), TypeError);
assertThrowsInstanceOf(() => Iterator.range(Symbol()), TypeError);

// Verify number and bigint inputs are accepted
Iterator.range(0);
Iterator.range(0n);

if (typeof reportCompare === 'function') {
    reportCompare(0, 0);
}
