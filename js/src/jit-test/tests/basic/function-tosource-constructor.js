var f = Function("a", "b", "return a + b;");
assertEq(f.toString(), "function anonymous(a, b) {\nreturn a + b;\n}");
assertEq(f.toSource(), "(function anonymous(a, b) {\nreturn a + b;\n})");
assertEq(decompileFunction(f), f.toString());
assertEq(decompileBody(f), "return a + b;");
f = Function("a", "...rest", "return rest[42] + b;");
assertEq(f.toString(), "function anonymous(a, ...rest) {\nreturn rest[42] + b;\n}");
assertEq(f.toSource(), "(function anonymous(a, ...rest) {\nreturn rest[42] + b;\n})")
assertEq(decompileFunction(f), f.toString());
assertEq(decompileBody(f), "return rest[42] + b;");
f = Function("");
assertEq(f.toString(), "function anonymous() {\n\n}");
f = Function("", "(abc)");
assertEq(f.toString(), "function anonymous() {\n(abc)\n}");
f = Function("", "return function (a, b) a + b;")();
assertEq(f.toString(), "function (a, b) a + b");
