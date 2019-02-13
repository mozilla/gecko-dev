// |reftest| skip-if(!xulRuntime.shell)
// Classes
function classesEnabled() {
    try {
        Reflect.parse("class foo { constructor() { } }");
        return true;
    } catch (e) {
        assertEq(e instanceof SyntaxError, true);
        return false;
    }
}

function testClasses() {
    function methodFun(id, kind, generator, args, body = []) {
        assertEq(generator && kind === "method", generator);
        let idN = ident(id);
        let methodMaker = generator ? genFunExpr : funExpr;
        let methodName = kind !== "method" ? null : idN;
        return methodMaker(methodName, args.map(ident), blockStmt(body));
    }

    function simpleMethod(id, kind, generator, args=[], isStatic=false) {
        return classMethod(ident(id),
                           methodFun(id, kind, generator, args),
                           kind, isStatic);
    }
    function emptyCPNMethod(id, isStatic) {
        return classMethod(computedName(lit(id)),
                           funExpr(null, [], blockStmt([])),
                           "method", isStatic);
    }

    function assertClassExpr(str, methods, heritage=null, name=null) {
        let template = classExpr(name, heritage, methods);
        assertExpr("(" + str + ")", template);
    }
    function assertClass(str, methods, heritage=null) {
        let namelessStr = str.replace("NAME", "");
        let namedStr = str.replace("NAME", "Foo");
        assertClassExpr(namelessStr, methods, heritage);
        assertClassExpr(namedStr, methods, heritage, ident("Foo"));

        let template = classStmt(ident("Foo"), heritage, methods);
        assertStmt(namedStr, template);
    }
    function assertNamedClassError(str, error) {
        assertError(str, error);
        assertError("(" + str + ")", error);
    }
    function assertClassError(str, error) {
        assertNamedClassError(str, error);
        assertError("(" + str.replace("NAME", "") + ")", error);
    }

    let simpleConstructor = simpleMethod("constructor", "method", false);

    /* Trivial classes */
    // Unnamed class statements are forbidden, but unnamed class expressions are
    // just fine.
    assertError("class { constructor() { } }", SyntaxError);
    assertClass("class NAME { constructor() { } }", [simpleConstructor]);

    // A class name must actually be a name
    assertNamedClassError("class x.y { constructor() {} }", SyntaxError);
    assertNamedClassError("class []  { constructor() {} }", SyntaxError);
    assertNamedClassError("class {x} { constructor() {} }", SyntaxError);
    assertNamedClassError("class for { constructor() {} }", SyntaxError);

    // Allow methods and accessors
    assertClass("class NAME { constructor() { } method() { } }",
                [simpleConstructor, simpleMethod("method", "method", false)]);

    assertClass("class NAME { constructor() { } get method() { } }",
                [simpleConstructor, simpleMethod("method", "get", false)]);

    assertClass("class NAME { constructor() { } set method(x) { } }",
                [simpleConstructor, simpleMethod("method", "set", false, ["x"])]);

    /* Static */
    assertClass(`class NAME {
                   constructor() { };
                   static method() { };
                   static *methodGen() { };
                   static get getter() { };
                   static set setter(x) { }
                 }`,
                [simpleConstructor,
                 simpleMethod("method", "method", false, [], true),
                 simpleMethod("methodGen", "method", true, [], true),
                 simpleMethod("getter", "get", false, [], true),
                 simpleMethod("setter", "set", false, ["x"], true)]);

    // It's not an error to have a method named static, static, or not.
    assertClass("class NAME { constructor() { } static() { } }",
                [simpleConstructor, simpleMethod("static", "method", false)]);
    assertClass("class NAME { static static() { }; constructor() { } }",
                [simpleMethod("static", "method", false, [], true), simpleConstructor]);
    assertClass("class NAME { static get static() { }; constructor() { } }",
                [simpleMethod("static", "get", false, [], true), simpleConstructor]);
    assertClass("class NAME { constructor() { }; static set static(x) { } }",
                [simpleConstructor, simpleMethod("static", "set", false, ["x"], true)]);

    // You do, however, have to put static in the right spot
    assertClassError("class NAME { constructor() { }; get static foo() { } }", SyntaxError);

    // Spec disallows "prototype" as a static member in a class, since that
    // one's important to make the desugaring work
    assertClassError("class NAME { constructor() { } static prototype() { } }", SyntaxError);
    assertClassError("class NAME { constructor() { } static *prototype() { } }", SyntaxError);
    assertClassError("class NAME { static get prototype() { }; constructor() { } }", SyntaxError);
    assertClassError("class NAME { static set prototype(x) { }; constructor() { } }", SyntaxError);

    // You are, however, allowed to have a CPN called prototype as a static
    assertClass("class NAME { constructor() { }; static [\"prototype\"]() { } }",
                [simpleConstructor, emptyCPNMethod("prototype", true)]);

    /* Constructor */
    // Currently, we do not allow default constructors
    assertClassError("class NAME { }", TypeError);

    // For now, disallow arrow functions in derived class constructors
    assertClassError("class NAME extends null { constructor() { (() => 0); }", InternalError);

    // It is an error to have two methods named constructor, but not other
    // names, regardless if one is an accessor or a generator or static.
    assertClassError("class NAME { constructor() { } constructor(a) { } }", SyntaxError);
    let methods = [["method() { }", simpleMethod("method", "method", false)],
                   ["*method() { }", simpleMethod("method", "method", true)],
                   ["get method() { }", simpleMethod("method", "get", false)],
                   ["set method(x) { }", simpleMethod("method", "set", false, ["x"])],
                   ["static method() { }", simpleMethod("method", "method", false, [], true)],
                   ["static *method() { }", simpleMethod("method", "method", true, [], true)],
                   ["static get method() { }", simpleMethod("method", "get", false, [], true)],
                   ["static set method(x) { }", simpleMethod("method", "set", false, ["x"], true)]];
    let i,j;
    for (i=0; i < methods.length; i++) {
        for (j=0; j < methods.length; j++) {
            let str = "class NAME { constructor() { } " +
                       methods[i][0] + " " + methods[j][0] +
                       " }";
            assertClass(str, [simpleConstructor, methods[i][1], methods[j][1]]);
        }
    }

    // It is, however, not an error to have a constructor, and a method with a
    // computed property name 'constructor'
    assertClass("class NAME { constructor () { } [\"constructor\"] () { } }",
                [simpleConstructor, emptyCPNMethod("constructor", false)]);

    // It is an error to have a generator or accessor named constructor
    assertClassError("class NAME { *constructor() { } }", SyntaxError);
    assertClassError("class NAME { get constructor() { } }", SyntaxError);
    assertClassError("class NAME { set constructor() { } }", SyntaxError);

    /* Semicolons */
    // Allow Semicolons in Class Definitions
    assertClass("class NAME { constructor() { }; }", [simpleConstructor]);

    // Allow more than one semicolon, even in otherwise trivial classses
    assertClass("class NAME { ;;; constructor() { } }", [simpleConstructor]);

    // Semicolons are optional, even if the methods share a line
    assertClass("class NAME { method() { } constructor() { } }",
                [simpleMethod("method", "method", false), simpleConstructor]);

    /* Generators */
    // No yield as a class name inside a generator
    assertError(`function *foo() {
                    class yield {
                        constructor() { }
                    }
                 }`, SyntaxError);
    assertError(`function *foo() {
                    (class yield {
                        constructor() { }
                    })
                 }`, SyntaxError);

    // Methods may be generators, but not accessors
    assertClassError("class NAME { constructor() { } *get foo() { } }", SyntaxError);
    assertClassError("class NAME { constructor() { } *set foo() { } }", SyntaxError);

    assertClass("class NAME { *method() { } constructor() { } }",
                [simpleMethod("method", "method", true), simpleConstructor]);

    /* Strictness */
    // yield is a strict-mode keyword, and class definitions are always strict.
    assertClassError("class NAME { constructor() { var yield; } }", SyntaxError);

    // Beware of the strictness of computed property names. Here use bareword
    // deletion (a deprecated action) to check.
    assertClassError("class NAME { constructor() { } [delete bar]() { }}", SyntaxError);

    /* Bindings */
    // Class statements bind lexically, so they should collide with other
    // in-block lexical bindings, but class expressions don't.
    assertError("{ let Foo; class Foo { constructor() { } } }", TypeError);
    assertStmt("{ let Foo; (class Foo { constructor() { } }) }",
               blockStmt([letDecl([{id: ident("Foo"), init: null}]),
                          exprStmt(classExpr(ident("Foo"), null, [simpleConstructor]))]));
    assertError("{ const Foo = 0; class Foo { constructor() { } } }", TypeError);
    assertStmt("{ const Foo = 0; (class Foo { constructor() { } }) }",
               blockStmt([constDecl([{id: ident("Foo"), init: lit(0)}]),
                          exprStmt(classExpr(ident("Foo"), null, [simpleConstructor]))]));
    assertError("{ class Foo { constructor() { } } class Foo { constructor() { } } }", TypeError);
    assertStmt(`{
                    (class Foo {
                        constructor() { }
                     },
                     class Foo {
                        constructor() { }
                     });
                }`,
               blockStmt([exprStmt(seqExpr([classExpr(ident("Foo"), null, [simpleConstructor]),
                                            classExpr(ident("Foo"), null, [simpleConstructor])]))]));
    assertStmt(`{
                    var x = class Foo { constructor() { } };
                    class Foo { constructor() { } }
                }`,
               blockStmt([varDecl([{ id: ident("x"),
                                     init: classExpr(ident("Foo"), null, [simpleConstructor])
                                   }]),
                          classStmt(ident("Foo"), null, [simpleConstructor])]));


    // Can't make a lexical binding without a block.
    assertError("if (1) class Foo { constructor() { } }", SyntaxError);

    /* Heritage Expressions */
    // It's illegal to have things that look like "multiple inheritance":
    // non-parenthesized comma expressions.
    assertClassError("class NAME extends null, undefined { constructor() { } }", SyntaxError);

    // Again check for strict-mode in heritage expressions
    assertClassError("class NAME extends (delete x) { constructor() { } }", SyntaxError);

    // You must specify an inheritance if you say "extends"
    assertClassError("class NAME extends { constructor() { } }", SyntaxError);

    // "extends" is still a valid name for a method
    assertClass("class NAME { constructor() { }; extends() { } }",
                [simpleConstructor, simpleMethod("extends", "method", false)]);

    // Immediate expression
    assertClass("class NAME extends null { constructor() { } }",
                [simpleConstructor], lit(null));

    // Sequence expresson
    assertClass("class NAME extends (undefined, undefined) { constructor() { } }",
                [simpleConstructor], seqExpr([ident("undefined"), ident("undefined")]));

    // Function expression
    let emptyFunction = funExpr(null, [], blockStmt([]));
    assertClass("class NAME extends function(){ } { constructor() { } }",
                [simpleConstructor], emptyFunction);

    // New expression
    assertClass("class NAME extends new function(){ }() { constructor() { } }",
                [simpleConstructor], newExpr(emptyFunction, []));

    // Call expression
    assertClass("class NAME extends function(){ }() { constructor() { } }",
                [simpleConstructor], callExpr(emptyFunction, []));

    // Dot expression
    assertClass("class NAME extends {}.foo { constructor() { } }",
                [simpleConstructor], dotExpr(objExpr([]), ident("foo")));

    // Member expression
    assertClass("class NAME extends {}[foo] { constructor() { } }",
                [simpleConstructor], memExpr(objExpr([]), ident("foo")));

    /* SuperProperty */
    // NOTE: Some of these tests involve object literals, as SuperProperty is a
    // valid production in any method definition, including in objectl
    // litterals. These tests still fall here, as |super| is not implemented in
    // any form without classes.

    function assertValidSuperProps(assertion, makeStr, makeExpr, type, generator, args, static,
                                   extending)
    {
        let superProperty = superProp(ident("prop"));
        let superMember = superElem(lit("prop"));

        let situations = [
            ["super.prop", superProperty],
            ["super['prop']", superMember],
            ["super.prop()", callExpr(superProperty, [])],
            ["super['prop']()", callExpr(superMember, [])],
            ["new super.prop()", newExpr(superProperty, [])],
            ["new super['prop']()", newExpr(superMember, [])],
            ["delete super.prop", unExpr("delete", superProperty)],
            ["delete super['prop']", unExpr("delete", superMember)],
            ["++super.prop", updExpr("++", superProperty, true)],
            ["super['prop']--", updExpr("--", superMember, false)],
            ["super.prop = 4", aExpr("=", superProperty, lit(4))],
            ["super['prop'] = 4", aExpr("=", superMember, lit(4))],
            ["super.prop += 4", aExpr("+=", superProperty, lit(4))],
            ["super['prop'] += 4", aExpr("+=", superMember, lit(4))],
            ["super.prop -= 4", aExpr("-=", superProperty, lit(4))],
            ["super['prop'] -= 4", aExpr("-=", superMember, lit(4))],
            ["super.prop *= 4", aExpr("*=", superProperty, lit(4))],
            ["super['prop'] *= 4", aExpr("*=", superMember, lit(4))],
            ["super.prop /= 4", aExpr("/=", superProperty, lit(4))],
            ["super['prop'] /= 4", aExpr("/=", superMember, lit(4))],
            ["super.prop %= 4", aExpr("%=", superProperty, lit(4))],
            ["super['prop'] %= 4", aExpr("%=", superMember, lit(4))],
            ["super.prop <<= 4", aExpr("<<=", superProperty, lit(4))],
            ["super['prop'] <<= 4", aExpr("<<=", superMember, lit(4))],
            ["super.prop >>= 4", aExpr(">>=", superProperty, lit(4))],
            ["super['prop'] >>= 4", aExpr(">>=", superMember, lit(4))],
            ["super.prop >>>= 4", aExpr(">>>=", superProperty, lit(4))],
            ["super['prop'] >>>= 4", aExpr(">>>=", superMember, lit(4))],
            ["super.prop |= 4", aExpr("|=", superProperty, lit(4))],
            ["super['prop'] |= 4", aExpr("|=", superMember, lit(4))],
            ["super.prop ^= 4", aExpr("^=", superProperty, lit(4))],
            ["super['prop'] ^= 4", aExpr("^=", superMember, lit(4))],
            ["super.prop &= 4", aExpr("&=", superProperty, lit(4))],
            ["super['prop'] &= 4", aExpr("&=", superMember, lit(4))],

            // We can also use super from inside arrow functions in method
            // definitions
            ["()=>super.prop", arrowExpr([], superProperty)],
            ["()=>super['prop']", arrowExpr([], superMember)]];

        for (let situation of situations) {
            let sitStr = situation[0];
            let sitExpr = situation[1];

            let fun = methodFun("method", type, generator, args, [exprStmt(sitExpr)]);
            let str = makeStr(sitStr, type, generator, args, static, extending);
            assertion(str, makeExpr(fun, type, static), extending);
        }
    }

    function assertValidSuperPropTypes(assertion, makeStr, makeExpr, static, extending) {
        for (let type of ["method", "get", "set"]) {
            if (type === "method") {
                // methods can also be generators
                assertValidSuperProps(assertion, makeStr, makeExpr, type, true, [], static, extending);
                assertValidSuperProps(assertion, makeStr, makeExpr, type, false, [], static, extending);
                continue;
            }

            // Setters require 1 argument, and getters require 0
            assertValidSuperProps(assertion, makeStr, makeExpr, type, false,
                                  type === "set" ? ["x"] : [], static, extending);
        }
    }

    function makeSuperPropMethodStr(propStr, type, generator, args) {
        return `${type === "method" ? "" : type} ${generator ? "*" : ""} method(${args.join(",")})
                {
                    ${propStr};
                }`;
    }

    function makeClassSuperPropStr(propStr, type, generator, args, static, extending) {
        return `class NAME ${extending ? "extends null" : ""} {
                    constructor() { };
                    ${static ? "static" : ""} ${makeSuperPropMethodStr(propStr, type, generator, args)}
                }`;
    }
    function makeClassSuperPropExpr(fun, type, static) {
        // We are going right into assertClass, so we don't have to build the
        // entire statement.
        return [simpleConstructor,
                classMethod(ident("method"), fun, type, static)];
    }
    function doClassSuperPropAssert(str, expr, extending) {
        assertClass(str, expr, extending ? lit(null) : null);
    }
    function assertValidClassSuperPropExtends(extending) {
        // super.prop and super[prop] are valid, regardless of whether the
        // method is static or not
        assertValidSuperPropTypes(doClassSuperPropAssert, makeClassSuperPropStr, makeClassSuperPropExpr,
                                  false, extending);
        assertValidSuperPropTypes(doClassSuperPropAssert, makeClassSuperPropStr, makeClassSuperPropExpr,
                                  true, extending);
    }
    function assertValidClassSuperProps() {
        // super.prop and super[prop] are valid, regardless of class heritage
        assertValidClassSuperPropExtends(false);
        assertValidClassSuperPropExtends(true);
    }

    function makeOLSuperPropStr(propStr, type, generator, args) {
        let str = `({ ${makeSuperPropMethodStr(propStr, type, generator, args)} })`;
        return str;
    }
    function makeOLSuperPropExpr(fun) {
        return objExpr([{ type: "Property", key: ident("method"), value: fun}]);
    }
    function assertValidOLSuperProps() {
        assertValidSuperPropTypes(assertExpr, makeOLSuperPropStr, makeOLSuperPropExpr);
    }


    // Check all valid uses of SuperProperty
    assertValidClassSuperProps();
    assertValidOLSuperProps();

    // SuperProperty is forbidden outside of method definitions.
    assertError("function foo () { super.prop; }", SyntaxError);
    assertError("(function () { super.prop; }", SyntaxError);
    assertError("(()=>super.prop)", SyntaxError);
    assertError("function *foo() { super.prop; }", SyntaxError);
    assertError("super.prop", SyntaxError);
    assertError("function foo () { super['prop']; }", SyntaxError);
    assertError("(function () { super['prop']; }", SyntaxError);
    assertError("(()=>super['prop'])", SyntaxError);
    assertError("function *foo() { super['prop']; }", SyntaxError);
    assertError("super['prop']", SyntaxError);

    // Or inside functions inside method definitions...
    assertClassError("class NAME { constructor() { function nested() { super.prop; }}}", SyntaxError);

    // Bare super is forbidden
    assertError("super", SyntaxError);

    // Even where super is otherwise allowed
    assertError("{ foo() { super } }", SyntaxError);
    assertClassError("class NAME { constructor() { super; } }", SyntaxError);

    /* EOF */
    // Clipped classes should throw a syntax error
    assertClassError("class NAME {", SyntaxError);
    assertClassError("class NAME {;", SyntaxError);
    assertClassError("class NAME { constructor", SyntaxError);
    assertClassError("class NAME { constructor(", SyntaxError);
    assertClassError("class NAME { constructor()", SyntaxError);
    assertClassError("class NAME { constructor()", SyntaxError);
    assertClassError("class NAME { constructor() {", SyntaxError);
    assertClassError("class NAME { constructor() { }", SyntaxError);
    assertClassError("class NAME { static", SyntaxError);
    assertClassError("class NAME { static y", SyntaxError);
    assertClassError("class NAME { static *", SyntaxError);
    assertClassError("class NAME { static *y", SyntaxError);
    assertClassError("class NAME { static get", SyntaxError);
    assertClassError("class NAME { static get y", SyntaxError);
    assertClassError("class NAME extends", SyntaxError);
    assertClassError("class NAME { constructor() { super", SyntaxError);
    assertClassError("class NAME { constructor() { super.", SyntaxError);
    assertClassError("class NAME { constructor() { super.x", SyntaxError);
    assertClassError("class NAME { constructor() { super.m(", SyntaxError);
    assertClassError("class NAME { constructor() { super[", SyntaxError);
    assertClassError("class NAME { constructor() { super(", SyntaxError);

    // Can not omit curly brackets
    assertClassError("class NAME { constructor() ({}) }", SyntaxError);
    assertClassError("class NAME { constructor() void 0 }", SyntaxError);
    assertClassError("class NAME { constructor() 1 }", SyntaxError);
    assertClassError("class NAME { constructor() false }", SyntaxError);
    assertClassError("class NAME { constructor() {} a() ({}) }", SyntaxError);
    assertClassError("class NAME { constructor() {} a() void 0 }", SyntaxError);
    assertClassError("class NAME { constructor() {} a() 1 }", SyntaxError);
    assertClassError("class NAME { constructor() {} a() false }", SyntaxError);

}

if (classesEnabled())
    runtest(testClasses);
else if (typeof reportCompare === 'function')
    reportCompare(true, true);
