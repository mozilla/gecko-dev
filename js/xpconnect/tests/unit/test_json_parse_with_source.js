'use strict';

const { addDebuggerToGlobal } = ChromeUtils.importESModule(
  "resource://gre/modules/jsdebugger.sys.mjs"
);

const SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"].createInstance(
  Ci.nsIPrincipal
);

function addTestingFunctionsToGlobal(global) {
  global.eval(
    `
      const testingFunctions = Cu.getJSTestingFunctions();
      for (let k in testingFunctions) {

        this[k] = testingFunctions[k];
      }
      `
  );
  if (!global.print) {
    global.print = info;
  }
  if (!global.newGlobal) {
    global.newGlobal = newGlobal;
  }
  if (!global.Debugger) {
    addDebuggerToGlobal(global);
  }
}

addTestingFunctionsToGlobal(this);

/* Create a new global, with all the JS shell testing functions. Similar to the
 * newGlobal function exposed to JS shells, and useful for porting JS shell
 * tests to xpcshell tests.
 */
function newGlobal(args) {
  const global = new Cu.Sandbox(SYSTEM_PRINCIPAL, {
    freshCompartment: true,
    ...args,
  });
  addTestingFunctionsToGlobal(global);
  return global;
}

add_task(function test_json_parse_with_source_xrays() {
  let sandbox = new Cu.Sandbox("about:blank");

  var sourceWrapper = Cu.evalInSandbox("JSON.parse('5.0', (k,v,{source}) => ({src: source, val: v}));", sandbox);
  Assert.deepEqual(sourceWrapper, {src: "5.0", val: 5});
  sandbox.reviver = (k,v,{source}) => { return { orig: source }};
  sourceWrapper = Cu.evalInSandbox("JSON.parse('2.4', reviver);", sandbox);
  Assert.deepEqual(sourceWrapper, { orig: "2.4"});

  // Get rid of the extra global when experimental.json_parse_with_source pref is removed
  var g = newGlobal({ newCompartment: true });
  Cu.evalInSandbox(`
    let other = new Cu.Sandbox("about:blank");
    let rawWrapper = other.eval('JSON.rawJSON(4.32)');
  `, g);
  Assert.ok(g.eval("JSON.isRawJSON(rawWrapper);"));
  Assert.equal(Cu.evalInSandbox("JSON.stringify(rawWrapper)", g), "4.32");

  // rawJSON is a data property, so the Xray should hide it
  Assert.equal(g.eval("rawWrapper.wrappedJSObject.rawJSON"), "4.32");
  Assert.equal(g.eval("rawWrapper.rawJSON"), undefined);

  let src = Cu.evalInSandbox(`
    other.eval('JSON.parse("4.32", (k,v,{source}) => { return {source,v}})');
  `, g);
  Assert.ok(Cu.isXrayWrapper(src));
  Assert.deepEqual(src, {source:"4.32", v:4.32});
});
