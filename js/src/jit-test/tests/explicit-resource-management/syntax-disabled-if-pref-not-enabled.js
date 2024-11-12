// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --disable-explicit-resource-management

load(libdir + "asserts.js");

function assertThrowsSyntaxError(str, reflectParseOptions = undefined) {
    assertThrowsInstanceOf(() => Reflect.parse(str, reflectParseOptions), SyntaxError);
}

// Valid `using` syntaxes that should not work.
assertThrowsSyntaxError("{ using x = {} }");
assertThrowsSyntaxError("using x = {}", { target: "module" });
assertThrowsSyntaxError("{ using x = fn() }");
assertThrowsSyntaxError("{ using x = fn(); }");
assertThrowsSyntaxError("function f() { using x = fn(); }");
assertThrowsSyntaxError("switch (x) { case 1: using x = fn(); }");
assertThrowsSyntaxError("if (x == 1) { using x = fn(); }");
assertThrowsSyntaxError("for (let i = 0; i < 10; i++) { using x = fn(); }");
assertThrowsSyntaxError("for (const i of [1, 2, 3]) { using x = fn(); }");
assertThrowsSyntaxError("for (const i in {a: 1, b: 2}) { using x = fn(); }");
assertThrowsSyntaxError("function* gen() { using x = fn(); }");
assertThrowsSyntaxError("async function* fn() { using x = fn(); }");
assertThrowsSyntaxError("async function fn() { using x = fn(); }");
assertThrowsSyntaxError("class xyz { static { using x = fn(); } }");
assertThrowsSyntaxError("{ using a = fn1(), b = fn2(); }");
assertThrowsSyntaxError("{ using x = null }");
assertThrowsSyntaxError("{ using x = undefined }");
assertThrowsSyntaxError("{ for (using x of y) {} }");
assertThrowsSyntaxError("for (using x of y) {}");
assertThrowsSyntaxError("for await (using x of y) {}", { target: "module" });
assertThrowsSyntaxError("async function fn() { for await (using x of y) {} }");
assertThrowsSyntaxError("{ using await = {}; }");
assertThrowsSyntaxError("{ using yield = {}; }");
assertThrowsSyntaxError("{ using public = {}; }");
assertThrowsSyntaxError("{ using private = {}; }");
assertThrowsSyntaxError("{ using protected = {}; }");
assertThrowsSyntaxError("{ using static = {}; }");
assertThrowsSyntaxError("{ using as = {}; }");
assertThrowsSyntaxError("{ using async = {}; }");
assertThrowsSyntaxError("{ using implements = {}; }");
assertThrowsSyntaxError("{ using interface = {}; }");
assertThrowsSyntaxError("{ using package = {}; }");
assertThrowsSyntaxError("'use strict'; { using await = {}; }");
assertThrowsSyntaxError("for (using await of y) {}");
assertThrowsSyntaxError("for (using yield of y) {}");
assertThrowsSyntaxError("using async = {};", { target: "module" });
assertThrowsSyntaxError("await using async = {};", { target: "module" });
assertThrowsSyntaxError("async function fn() { using async = {}; }");

// Valid `await using` syntaxes that should not work.
assertThrowsSyntaxError("await using x = {}", { target: "module" });
assertThrowsSyntaxError("if (x == 1) { await using x = fn(); }", { target: "module" });
assertThrowsSyntaxError("switch (x) { case 1: await using x = fn(); }", { target: "module" });
assertThrowsSyntaxError("async function fn() { await using x = {}; }");
assertThrowsSyntaxError("async function* gen() { await using x = {}; }");
assertThrowsSyntaxError("for (let i = 0; i < 10; i++) { await using x = fn(); }", { target: "module" });
assertThrowsSyntaxError("for (const i of [1, 2, 3]) { await using x = fn(); }", { target: "module" });
assertThrowsSyntaxError("for (const i in {a: 1, b: 2}) { await using x = fn(); }", { target: "module" });
assertThrowsSyntaxError("for (await using x of y) {}", { target: "module" });
assertThrowsSyntaxError("for await (await using x of y) {}", { target: "module" });
assertThrowsSyntaxError("async function fn() { for (await using x of y) {} }");
assertThrowsSyntaxError("async function fn() { for await (await using x of y) {} }");
assertThrowsSyntaxError("async function fn() { await using yield = {} }");
assertThrowsSyntaxError("async function fn() { await using public = {} }");
assertThrowsSyntaxError("async function fn() { await using private = {} }");
assertThrowsSyntaxError("async function fn() { await using protected = {} }");
assertThrowsSyntaxError("async function fn() { await using static = {} }");
assertThrowsSyntaxError("async function fn() { await using as = {} }");
assertThrowsSyntaxError("async function fn() { await using async = {} }");
assertThrowsSyntaxError("async function fn() { await using implements = {} }");
assertThrowsSyntaxError("async function fn() { await using package = {}; }");
assertThrowsSyntaxError("await using as = {}", { target: "module" });
assertThrowsSyntaxError("await using async = {};", { target: "module" });
assertThrowsSyntaxError("for (await using of of y) {}", { target: "module" });

// Valid syntaxes close to `using` but not `using` declarations.
Reflect.parse("for (using of y) {}");
Reflect.parse("for (using of of) {}");
Reflect.parse("for (using\nof y) {}");
Reflect.parse("{ const using = 10; }");
Reflect.parse("{ let using = 10 }");
Reflect.parse("{ var using = 10 }");
Reflect.parse("{ using = 10 }");
Reflect.parse("{ using + 1 }");
Reflect.parse("{ using++ }");
Reflect.parse("{ using\nx = 10 }");
Reflect.parse("{ using = {x: 10} }");
Reflect.parse("{ x = { using: 10 } }");
Reflect.parse("{ x.using = 10 }");
Reflect.parse("{ x\n.using = 10 }");
Reflect.parse("{ using.x = 10 }");
Reflect.parse("{ using\n.x = 10 }");
Reflect.parse("for (using[1] of {}) {}");
Reflect.parse("for (using\n[1] of {}) {}")
Reflect.parse("for (using.x of {}) {}");
Reflect.parse("for (using\n.x of {}) {}");
Reflect.parse("for (x.using in {}) {}");
Reflect.parse("for (x\n.using in {}) {}")
Reflect.parse("{ using: x = 10; }");
Reflect.parse("{ using\n: x = 10; }");
Reflect.parse("{ using /a/g; }");
Reflect.parse("{ /using/g }");
Reflect.parse("{ using\nlet = {} }");
Reflect.parse("export const using = 10", { target: "module" });
Reflect.parse("import using from 'xyz'", { target: "module" });

const ast4 = Reflect.parse("{ using = 10 }");
assertEq(ast4.body[0].body[0].type, "ExpressionStatement");
assertEq(ast4.body[0].body[0].expression.type, "AssignmentExpression");
assertEq(ast4.body[0].body[0].expression.left.type, "Identifier");
assertEq(ast4.body[0].body[0].expression.left.name, "using");

const ast5 = Reflect.parse("for (using of y) {}");
assertEq(ast5.body[0].type, "ForOfStatement");
assertEq(ast5.body[0].left.type, "Identifier");
assertEq(ast5.body[0].left.name, "using");

const ast6 = Reflect.parse("{ using + 1 }");
assertEq(ast6.body[0].body[0].type, "ExpressionStatement");
assertEq(ast6.body[0].body[0].expression.type, "BinaryExpression");
assertEq(ast6.body[0].body[0].expression.left.type, "Identifier");
assertEq(ast6.body[0].body[0].expression.left.name, "using");

const ast7 = Reflect.parse("for (using of of) {}");
assertEq(ast7.body[0].type, "ForOfStatement");
assertEq(ast7.body[0].left.type, "Identifier");
assertEq(ast7.body[0].left.name, "using");
assertEq(ast7.body[0].right.type, "Identifier");
assertEq(ast7.body[0].right.name, "of");

const ast8 = Reflect.parse("for (using\nof y) {}");
assertEq(ast8.body[0].type, "ForOfStatement");
assertEq(ast8.body[0].left.type, "Identifier");
assertEq(ast8.body[0].left.name, "using");
assertEq(ast8.body[0].right.type, "Identifier");
assertEq(ast8.body[0].right.name, "y");

const ast9 = Reflect.parse("for (using[1] of {}) {}");
assertEq(ast9.body[0].type, "ForOfStatement");
assertEq(ast9.body[0].left.type, "MemberExpression");
assertEq(ast9.body[0].left.object.type, "Identifier");
assertEq(ast9.body[0].left.object.name, "using");
assertEq(ast9.body[0].left.property.type, "Literal");
assertEq(ast9.body[0].left.property.value, 1);

const ast10 = Reflect.parse("for (using\n[1] of {}) {}");
assertEq(ast10.body[0].type, "ForOfStatement");
assertEq(ast10.body[0].left.type, "MemberExpression");
assertEq(ast10.body[0].left.object.type, "Identifier");
assertEq(ast10.body[0].left.object.name, "using");
assertEq(ast10.body[0].left.property.type, "Literal");
assertEq(ast10.body[0].left.property.value, 1);

const ast11 = Reflect.parse("{ /using/g }");
assertEq(ast11.body[0].body[0].type, "ExpressionStatement");
assertEq(ast11.body[0].body[0].expression.type, "Literal");
assertEq(ast11.body[0].body[0].expression.value.source, "using");
assertEq(ast11.body[0].body[0].expression.value.flags, "g");

const ast12 = Reflect.parse("{ using: x = 10; }");
assertEq(ast12.body[0].body[0].type, "LabeledStatement");
assertEq(ast12.body[0].body[0].label.type, "Identifier");
assertEq(ast12.body[0].body[0].label.name, "using");

const ast13 = Reflect.parse("{ using\n: x = 10; }");
assertEq(ast13.body[0].body[0].type, "LabeledStatement");
assertEq(ast13.body[0].body[0].label.type, "Identifier");
assertEq(ast13.body[0].body[0].label.name, "using");

const ast14 = Reflect.parse("{ using /a/g; }");
// should be parsed as division, not regex
assertEq(ast14.body[0].body[0].type, "ExpressionStatement");
assertEq(ast14.body[0].body[0].expression.type, "BinaryExpression");
assertEq(ast14.body[0].body[0].expression.operator, "/");
assertEq(ast14.body[0].body[0].expression.left.type, "BinaryExpression");
assertEq(ast14.body[0].body[0].expression.left.operator, "/");

const ast15 = Reflect.parse("import using from 'xyz'", { target: "module" });
assertEq(ast15.body[0].type, "ImportDeclaration");
assertEq(ast15.body[0].specifiers[0].type, "ImportSpecifier");
assertEq(ast15.body[0].specifiers[0].name.name, "using");

// Valid syntaxes close to `await using` but not `await using` declarations.
Reflect.parse("await /xyz/g");
Reflect.parse("{ await /using/g }");
Reflect.parse("async function fn() { await using; }");
Reflect.parse("async function fn() { await\nusing; }")
Reflect.parse("async function fn() { await /using/g }");
Reflect.parse("async function fn() { await using/g }");
Reflect.parse("async function fn() { await using/\ng }");
Reflect.parse("async function fn() { for(await using;;) {} }");
Reflect.parse("await using;", { target: "module" });
Reflect.parse("await\nusing;", { target: "module" });
Reflect.parse("await /using/g", { target: "module" });
Reflect.parse("await using/g", { target: "module" });
Reflect.parse("await using/\ng", { target: "module" });
Reflect.parse("for(await using;;) {}", { target: "module" });
Reflect.parse("await using\nx;", { target: "module" });

const ast19 = Reflect.parse("await using", { target: "module" });
assertEq(ast19.body[0].type, "ExpressionStatement");
assertEq(ast19.body[0].expression.type, "UnaryExpression");
assertEq(ast19.body[0].expression.operator, "await");
assertEq(ast19.body[0].expression.argument.type, "Identifier");
assertEq(ast19.body[0].expression.argument.name, "using");

const ast20 = Reflect.parse("await /using/g", { target: "module" });
assertEq(ast20.body[0].type, "ExpressionStatement");
assertEq(ast20.body[0].expression.type, "UnaryExpression");
assertEq(ast20.body[0].expression.operator, "await");
assertEq(ast20.body[0].expression.argument.type, "Literal");
assertEq(ast20.body[0].expression.argument.value.source, "using");
assertEq(ast20.body[0].expression.argument.value.flags, "g");

const ast21 = Reflect.parse("await using/g", { target: "module" });
assertEq(ast21.body[0].type, "ExpressionStatement");
assertEq(ast21.body[0].expression.type, "BinaryExpression");
assertEq(ast21.body[0].expression.operator, "/");
assertEq(ast21.body[0].expression.left.type, "UnaryExpression");
assertEq(ast21.body[0].expression.left.operator, "await");
assertEq(ast21.body[0].expression.left.argument.type, "Identifier");
assertEq(ast21.body[0].expression.left.argument.name, "using");

const ast22 = Reflect.parse("for(await using;;) {}", { target: "module" });
assertEq(ast22.body[0].type, "ForStatement");
assertEq(ast22.body[0].init.type, "UnaryExpression");
assertEq(ast22.body[0].init.operator, "await");
assertEq(ast22.body[0].init.argument.type, "Identifier");
assertEq(ast22.body[0].init.argument.name, "using");
