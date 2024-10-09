// |jit-test| --enable-import-attributes

if (getRealmConfiguration("importAttributes")) {
  // Register a JS module with the specifier 'foo'.
  let fooJs = registerModule("foo", parseModule('export const test = true; export const test2 = 2;'));

  // Register a JSON module with the same specifier 'foo'.
  let fooJson = registerModule("foo", parseModule('{"test": true}', "", "json"));

  let a = registerModule("a", parseModule(`import {test} from "foo"; import json from "foo" with { type: "json" };`));
  moduleLink(a);
  moduleEvaluate(a);

  let json = getModuleEnvironmentValue(a, 'json');
  assertEq(json.test, true);

  let test = getModuleEnvironmentValue(a, 'test');
  assertEq(test, true);

  let expectedModules = [fooJs, fooJson];
  assertEq(a.requestedModules.length, expectedModules.length);
}
