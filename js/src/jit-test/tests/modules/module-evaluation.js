// Exercise ModuleEvaluation() concrete method.

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

function parseAndEvaluate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    m.evaluation();
    return m;
}

// Check the evaluation of an empty module succeeds.
parseAndEvaluate("");

// Check evaluation returns evaluation result the first time, then undefined.
let m = parseModule("1");
m.declarationInstantiation();
assertEq(m.evaluation(), undefined);
assertEq(typeof m.evaluation(), "undefined");

// Check top level variables are initialized by evaluation.
m = parseModule("export var x = 2 + 2;");
assertEq(typeof getModuleEnvironmentValue(m, "x"), "undefined");
m.declarationInstantiation();
m.evaluation();
assertEq(getModuleEnvironmentValue(m, "x"), 4);

m = parseModule("export let x = 2 * 3;");
m.declarationInstantiation();
m.evaluation();
assertEq(getModuleEnvironmentValue(m, "x"), 6);

// Set up a module to import from.
let a = moduleRepo['a'] =
parseModule(`var x = 1;
             export { x };
             export default 2;
             export function f(x) { return x + 1; }`);

// Check we can evaluate top level definitions.
parseAndEvaluate("var foo = 1;");
parseAndEvaluate("let foo = 1;");
parseAndEvaluate("const foo = 1");
parseAndEvaluate("function foo() {}");
parseAndEvaluate("class foo { constructor() {} }");

// Check we can evaluate all module-related syntax.
parseAndEvaluate("export var foo = 1;");
parseAndEvaluate("export let foo = 1;");
parseAndEvaluate("export const foo = 1;");
parseAndEvaluate("var x = 1; export { x };");
parseAndEvaluate("export default 1");
parseAndEvaluate("export default function() {};");
parseAndEvaluate("export default function foo() {};");
parseAndEvaluate("import a from 'a';");
parseAndEvaluate("import { x } from 'a';");
parseAndEvaluate("import * as ns from 'a';");
parseAndEvaluate("export * from 'a'");
parseAndEvaluate("export default class { constructor() {} };");
parseAndEvaluate("export default class foo { constructor() {} };");

// Test default import
m = parseModule("import a from 'a'; export { a };")
m.declarationInstantiation();
m.evaluation()
assertEq(getModuleEnvironmentValue(m, "a"), 2);

// Test named import
m = parseModule("import { x as y } from 'a'; export { y };")
m.declarationInstantiation();
m.evaluation();
assertEq(getModuleEnvironmentValue(m, "y"), 1);

// Call exported function
m = parseModule("import { f } from 'a'; export let x = f(3);")
m.declarationInstantiation();
m.evaluation();
assertEq(getModuleEnvironmentValue(m, "x"), 4);

// Test importing an indirect export
moduleRepo['b'] = parseModule("export { x as z } from 'a';");
m = parseAndEvaluate("import { z } from 'b'; export { z }");
assertEq(getModuleEnvironmentValue(m, "z"), 1);

// Test cyclic dependencies
moduleRepo['c1'] = parseModule("export var x = 1; export {y} from 'c2'");
moduleRepo['c2'] = parseModule("export var y = 2; export {x} from 'c1'");
m = parseAndEvaluate(`import { x as x1, y as y1 } from 'c1';
                      import { x as x2, y as y2 } from 'c2';
                      export let z = [x1, y1, x2, y2]`),
assertDeepEq(getModuleEnvironmentValue(m, "z"), [1, 2, 1, 2]);

// Import access in functions
m = parseModule("import { x } from 'a'; function f() { return x; }")
m.declarationInstantiation();
m.evaluation();
let f = getModuleEnvironmentValue(m, "f");
assertEq(f(), 1);
