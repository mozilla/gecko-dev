function testMixed() {
    var global = newGlobal({newCompartment: true});
    var wrapperEmulatesUndefined = global.evaluate("createIsHTMLDDA()");
    var wrapperPlain = global.evaluate("({})");

    var arr = [
        createIsHTMLDDA(),
        wrapperEmulatesUndefined,
        wrapperPlain,
        this,
        new Proxy({}, {})
    ];

    var res = 0;
    for (var i = 0; i < 100; i++) {
        var val = arr[i % arr.length];
        if (val) {
            res++;
        }
        if (val == null) {
            res++;
        }
        if (val != undefined) {
            res++;
        }
    }
    assertEq(res, 160);
}
testMixed();

function testNonWrapperProxy() {
    var proxies = [new Proxy({}, {}), new Proxy({}, {})];
    var res = 0;
    for (var i = 0; i < 100; i++) {
        var val = proxies[i % proxies.length];
        if (val) {
            res++;
        }
        if (val == null) {
            throw "failure";
        }
        if (val != undefined) {
            res++;
        }
    }
    assertEq(res, 200);
}
testNonWrapperProxy();
