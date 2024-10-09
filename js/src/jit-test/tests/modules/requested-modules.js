// |jit-test| --enable-import-attributes

// Test requestedModules property

function testRequestedModules(source, expected) {
    var module = parseModule(source);
    var actual = module.requestedModules;
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i].moduleRequest.specifier, expected[i].specifier);
        assertEq(actual[i].moduleRequest.moduleType, expected[i].moduleType);
        if (actual[i].moduleRequest.firstUnsupportedAttributeKey != expected[i].firstUnsupportedAttributeKey) {
          assertEq(true, false);
        }
    }
}

testRequestedModules("", []);

testRequestedModules("import a from 'foo'", [
    { specifier: 'foo', moduleType: 'js' }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'", [
    { specifier: 'foo', moduleType: 'js' },
    { specifier: 'bar', moduleType: 'js' }
]);

testRequestedModules("import a from 'foo'; import b from 'foo'", [
    { specifier: 'foo', moduleType: 'js' }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'; import c from 'foo'", [
    { specifier: 'foo', moduleType: 'js' },
    { specifier: 'bar', moduleType: 'js' }
]);

testRequestedModules("export {} from 'foo'", [
    { specifier: 'foo', moduleType: 'js' }
]);

testRequestedModules("export * from 'bar'",[
    { specifier: 'bar', moduleType: 'js' }
]);

testRequestedModules("import a from 'foo'; export {} from 'bar'; export * from 'baz'", [
    { specifier: 'foo', moduleType: 'js' },
    { specifier: 'bar', moduleType: 'js' },
    { specifier: 'baz', moduleType: 'js' }
]);

if (getRealmConfiguration("importAttributes")) {
    testRequestedModules("import a from 'foo' with {}", [
        { specifier: 'foo', moduleType: 'js' },
    ]);

    testRequestedModules("import a from 'foo' with { type: 'json'}", [
        { specifier: 'foo', moduleType: 'json' },
    ]);

    testRequestedModules("import a from 'foo' with { unsupported: 'test'}", [
        { specifier: 'foo', moduleType: 'js', firstUnsupportedAttributeKey: 'unsupported'},
    ]);

    testRequestedModules("import a from 'foo' with { unsupported: 'test', type: 'js', foo: 'bar' }", [
        { specifier: 'foo', moduleType: 'unknown', firstUnsupportedAttributeKey: 'unsupported' },
    ]);

    testRequestedModules("import a from 'foo' with { type: 'js1'}; export {} from 'bar' with { type: 'js2'}; export * from 'baz' with { type: 'js3'}", [
        { specifier: 'foo', moduleType: 'unknown' },
        { specifier: 'bar', moduleType: 'unknown' },
        { specifier: 'baz', moduleType: 'unknown' }
    ]);

    testRequestedModules("export {} from 'foo' with { type: 'js'}", [
        { specifier: 'foo', moduleType: 'unknown' }
    ]);

    testRequestedModules("export * from 'bar' with { type: 'json'}",[
        { specifier: 'bar', moduleType: 'json' }
    ]);

    testRequestedModules("import a from 'foo'; import b from 'bar' with { type: 'json' };", [
        { specifier: 'foo', moduleType: 'js' },
        { specifier: 'bar', moduleType: 'json' },
    ]);

    testRequestedModules("import a from 'foo'; import b from 'foo' with { type: 'json' };", [
        { specifier: 'foo', moduleType: 'js' },
        { specifier: 'foo', moduleType: 'json' },
    ]);

    testRequestedModules("import a from 'foo'; import b from 'foo' with { type: 'js1' };", [
        { specifier: 'foo', moduleType: 'js' },
        { specifier: 'foo', moduleType: 'unknown' },
    ]);

    testRequestedModules("import a from 'foo'; import b from 'foo' with { type: 'json' }; import c from 'foo' with { type: 'json' };", [
        { specifier: 'foo', moduleType: 'js' },
        { specifier: 'foo', moduleType: 'json' }
    ]);

    testRequestedModules("import a from 'foo'; import b from 'foo' with { type: 'json' }; import c from 'foo' with { type: 'json', foo: 'bar'};", [
        { specifier: 'foo', moduleType: 'js' },
        { specifier: 'foo', moduleType: 'json' },
        { specifier: 'foo', moduleType: 'json', firstUnsupportedAttributeKey: 'foo' }
    ]);

    testRequestedModules("import a from 'foo' with { type: 'json' }; import b from 'foo' with { type: 'json' };", [
        { specifier: 'foo', moduleType: 'json' },
    ]);

    testRequestedModules("import b from 'bar' with { type: 'json' }; import a from 'foo';", [
        { specifier: 'bar', moduleType: 'json' },
        { specifier: 'foo', moduleType: 'js' },
    ]);

    testRequestedModules("export {} from 'foo' with { type: 'someValueThatWillNeverBeSupported'}", [
        { specifier: 'foo', moduleType: 'unknown' }
    ]);
}
