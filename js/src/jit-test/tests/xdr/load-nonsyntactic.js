var nonSyntacticEnvironment = { field: 10 };

var source = `
function f() {
    function g() {
        function h() {
            // Load out of non syntactic environment.
            return field;
        }
        return h();
    }
    return g();
}

f()`

function check(before, after) {
    var code = cacheEntry(source);
    var res = evaluate(code, before);
    assertEq(res, 10);
    res = evaluate(code, after);
    assertEq(res, 10);
}


check({ envChainObject: nonSyntacticEnvironment, saveBytecodeWithDelazifications: true, },
    { envChainObject: nonSyntacticEnvironment, loadBytecode: true })


try {
    var global = newGlobal();
    global.field = 10;
    check({ envChainObject: nonSyntacticEnvironment, saveBytecodeWithDelazifications: true, },
        { global: global, loadBytecode: true })

    // Should have thrown
    assertEq(false, true)
} catch (e) {
    assertEq(/Incompatible cache contents/.test(e.message), true);
}

try {
    check({ global: global, saveBytecodeWithDelazifications: true },
        { envChainObject: nonSyntacticEnvironment, loadBytecode: true })

    // Should have thrown
    assertEq(false, true)
} catch (e) {
    assertEq(/Incompatible cache contents/.test(e.message), true);
}


var nonSyntacticEnvironmentTwo = { field: 10 };
check({ envChainObject: nonSyntacticEnvironment, saveBytecodeWithDelazifications: true, },
    { envChainObject: nonSyntacticEnvironmentTwo, loadBytecode: true })
