/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { dumpScope } = ChromeUtils.importESModule(
  "resource://devtools/shared/test-helpers/dump-scope.sys.mjs",
  { global: "devtools" }
);

function nestedSync1() {
  /* eslint-disable no-unused-vars */
  const a = { prop: 1 };
  const b = [1, 2, 3];
  const c = function () {};
  /* eslint-enable no-unused-vars */

  return nestedSync2();
}

function nestedSync2() {
  /* eslint-disable no-unused-vars */
  const d = { prop: -1 };
  const e = ["4", "5", "6"];
  const f = new Uint8Array([7, 8, 9]);
  /* eslint-enable no-unused-vars */

  return dumpScope({ saveAsFile: false });
}

add_task(async function testSyncStack() {
  const scopes = await nestedSync1();
  is(scopes.length, 3, "dumpScope returned 3 frames");

  // Test frame for nestedSync1
  const testScope1 = scopes[1];
  assertScopeInformation(testScope1);

  let scope = testScope1.blocks[0];
  is(typeof scope.a, "object", "Object type is correct");
  is(scope.a.prop, "1", "Object property value is correct");

  is(typeof scope.b, "object");
  ok(Array.isArray(scope.b));
  is(scope.b[0], "1", "Array of number value is correct");
  is(scope.b[1], "2", "Array of number value is correct");
  is(scope.b[2], "3", "Array of number value is correct");

  is(typeof scope.c, "string", "Function type is correct");
  is(scope.c, "Function c", "Function value is correct");

  // Test frame for nestedSync2
  const testScope2 = scopes[0];
  assertScopeInformation(testScope2);

  scope = testScope2.blocks[0];
  is(typeof scope.d, "object", "Object type is correct");
  is(scope.d.prop, "-1", "Object property value is correct");

  // Check regular array of strings
  is(typeof scope.e, "object");
  ok(Array.isArray(scope.e), "Array of string type is correct");
  is(scope.e[0], "4", "Array of string value is correct");
  is(scope.e[1], "5", "Array of string value is correct");
  is(scope.e[2], "6", "Array of string value is correct");

  // Check typed array
  is(typeof scope.e, "object");
  ok(Array.isArray(scope.f), "Typed array type is correct");
  is(scope.f[0], "7", "Typed array value is correct");
  is(scope.f[1], "8", "Typed array value is correct");
  is(scope.f[2], "9", "Typed array value is correct");
});

async function nestedAsync1() {
  // eslint-disable-next-line no-unused-vars
  const asyncA = 1;
  const scopes = await nestedAsync2();
  return scopes;
}

async function nestedAsync2() {
  // eslint-disable-next-line no-unused-vars
  const asyncB = 2;
  const scopes = await nestedAsync3();
  return scopes;
}

async function nestedAsync3() {
  // eslint-disable-next-line no-unused-vars
  const asyncC = 3;
  return dumpScope({ saveAsFile: false });
}

add_task(async function testAsyncStack() {
  const scopes = await nestedAsync1();
  is(scopes.length, 4, "dumpScope returned 4 frames");

  const testScope1 = scopes[2];
  assertScopeInformation(testScope1);
  is(testScope1.blocks[0].asyncA, "1", "Async frame value is correct");
  const testScope2 = scopes[1];
  assertScopeInformation(testScope2);
  is(testScope2.blocks[0].asyncB, "2", "Async frame value is correct");
  const testScope3 = scopes[0];
  assertScopeInformation(testScope3);
  is(testScope3.blocks[0].asyncC, "3", "Async frame value is correct");
});

function cyclicReference() {
  const a = {};
  const b = { a };
  a.b = b;

  return dumpScope({ saveAsFile: false });
}

add_task(async function testCyclicReference() {
  const scopes = await cyclicReference();
  is(scopes.length, 2, "dumpScope returned 2 frames");

  const testScope = scopes[0];
  assertScopeInformation(testScope);
  is(typeof testScope.blocks[0].a, "object", "Cyclic object type is correct");

  // Note that the actual way cyclic references are handled could change in the
  // future. If we decide to start handling them as references, this test should
  // simply be updated!
  is(typeof testScope.blocks[0].a.b, "object", "Cyclic object type is correct");
  is(
    typeof testScope.blocks[0].a.b.a,
    "object",
    "Cyclic object type is correct"
  );
  is(
    typeof testScope.blocks[0].a.b.a.b,
    "object",
    "Cyclic object type is correct"
  );
  is(
    testScope.blocks[0].a.b.a.b.a,
    "Object (max depth)",
    "Cyclic object value is correct"
  );
});

function maxDepth() {
  const obj = {};

  let o = obj;
  for (let i = 0; i < 100; i++) {
    o = o[`a${i}`] = {};
  }

  return dumpScope({ saveAsFile: false });
}

add_task(async function testMaxDepth() {
  const scopes = await maxDepth();
  is(scopes.length, 2, "dumpScope returned 2 frames");

  const testScope = scopes[0];
  assertScopeInformation(testScope);
  is(
    typeof testScope.blocks[0].obj,
    "object",
    "Max depth object type is correct"
  );
  is(
    typeof testScope.blocks[0].obj.a0,
    "object",
    "Max depth object type is correct"
  );
  is(
    typeof testScope.blocks[0].obj.a0.a1,
    "object",
    "Max depth object type is correct"
  );
  is(
    typeof testScope.blocks[0].obj.a0.a1.a2,
    "object",
    "Max depth object type is correct"
  );
  is(
    typeof testScope.blocks[0].obj.a0.a1.a2.a3,
    "string",
    "Max depth object type is correct"
  );
  is(
    testScope.blocks[0].obj.a0.a1.a2.a3,
    "Object (max depth)",
    "Max depth object value is correct"
  );
});

function nestedInABlock() {
  /* eslint-disable no-unused-vars */
  const outerBlock = "outerBlock";

  {
    const innerBlock = "innerBlock";
    return dumpScope({ saveAsFile: false });
  }
  /* eslint-enable no-unused-vars */
}

add_task(async function testBlockNesting() {
  const scopes = await nestedInABlock();
  is(scopes.length, 2, "dumpScope returned 2 frames");

  // Test frame for nestedInABlock
  const testScope1 = scopes[0];
  assertScopeInformation(testScope1);

  const block1 = testScope1.blocks[0];
  is(typeof block1.innerBlock, "string", "innerBlock type is correct");
  is(block1.innerBlock, "innerBlock", "innerBlock value is correct");
  const block2 = testScope1.blocks[1];
  is(typeof block2.outerBlock, "string", "outerBlock type is correct");
  is(block2.outerBlock, "outerBlock", "outerBlock value is correct");
});

function uninitialized() {
  /* eslint-disable no-unused-vars */
  const scopes = dumpScope({ saveAsFile: false });
  const afterCallToDumpScope = "afterCallToDumpScope";
  return scopes;
  /* eslint-enable no-unused-vars */
}

add_task(async function testUninitialized() {
  const scopes = await uninitialized();
  is(scopes.length, 2, "dumpScope returned 2 frames");

  // Test frame for uninitialized
  const testScope1 = scopes[0];
  assertScopeInformation(testScope1);

  const block1 = testScope1.blocks[0];
  is(
    block1.afterCallToDumpScope,
    "(uninitialized)",
    "variable initialized after the call to dumpScope is marked as (uninitialized)"
  );
});

/**
 * Basic test helper to assert the shape of the frame object
 */
function assertScopeInformation(scopeInfo) {
  is(
    scopeInfo.details.frameScriptUrl,
    "chrome://mochitests/content/browser/devtools/shared/test-helpers/browser_dump_scope.js"
  );
  is(typeof scopeInfo.details.columnNumber, "number");
  is(typeof scopeInfo.details.lineNumber, "number");
  ok(Array.isArray(scopeInfo.blocks));
  for (const block of scopeInfo.blocks) {
    is(typeof block, "object");
  }
}
