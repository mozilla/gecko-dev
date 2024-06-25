const v3 = "6 Jun 2014" + 1000000.0;
for (let i4 = v3; i4-- > 536870888;) {
}
const v10 = ` 
function m(stdlib) {
    "use asm";
    var abs = stdlib.Math.abs;
    function f(d) {
        d = +d;
        return (~~(5.0 - +abs(d)))|0;
    }
    return f;
}`;
const o11 = { 
};
o11.lineNumber = 2096777049;
evaluate(cacheEntry(v10), o11);
