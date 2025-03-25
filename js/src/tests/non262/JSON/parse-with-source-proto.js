// |reftest| shell-option(--enable-json-parse-with-source) skip-if(!JSON.hasOwnProperty('isRawJSON')||!xulRuntime.shell)

// Changing Object.prototype should not change the behavior of JSON parsing.
Object.defineProperty(Object.prototype, 1, {
    set(val) {
        this[0] = {}
    },
})
assertDeepEq(["a", "b"], JSON.parse('["a","b"]', (k,v,c) => v))

if (typeof reportCompare == 'function')
    reportCompare(0, 0);