for (let name of ["test", Symbol.match, Symbol.replace, Symbol.search]) {
    let methodName = typeof name === "symbol" ? `[${name.description}]` : name;
    assertThrowsInstanceOfWithMessage(
        () => RegExp.prototype[name].call({}),
        TypeError,
        `${methodName} method called on incompatible Object`);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
