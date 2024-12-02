// |reftest| skip-if(!xulRuntime.shell) async  -- needs drainJobQueue
// SKIP test262 export
// Test needs drainJobQueue.

async function test() {
  try {
    await import("./bug1689499-a.js");
    throw new Error("import should have failed");
  } catch (exc) {
    assertEq(exc.message, "FAIL");
  }

  try {
    await import("./bug1689499-x.js");
    throw new Error("import should have failed");
  } catch (exc) {
    assertEq(exc.message, "FAIL");
  }

  if (typeof reportCompare === "function")
        reportCompare(0, 0);
}

test();
drainJobQueue();
